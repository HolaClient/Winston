use napi_derive::napi;
use std::net::TcpListener;
use std::thread::{self, JoinHandle};
use std::sync::Arc;
use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};

const RESPONSE: &[u8] = b"HTTP/1.1 200 OK\r\n\
    Content-Length: 13\r\n\
    Content-Type: text/plain\r\n\
    Connection: keep-alive\r\n\
    \r\n\
    Hello, World!\n";

#[napi]
pub struct Server {
    running: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

#[napi]
impl Server {
    #[napi(constructor)]
    pub fn new() -> Self {
        Server {
            running: Arc::new(AtomicBool::new(false)),
            handle: None,
        }
    }

    #[napi]
    pub fn listen(&mut self, port: i32) -> napi::Result<()> {
        if self.running.load(Ordering::SeqCst) {
            return Err(napi::Error::from_reason("Server already running"));
        }

        let addr = format!("0.0.0.0:{}", port);
        let listener = TcpListener::bind(&addr)
            .map_err(|e| napi::Error::from_reason(format!("Failed to bind: {}", e)))?;

        println!("Server listening on {}", addr);
        let running = self.running.clone();
        running.store(true, Ordering::SeqCst);

        self.handle = Some(thread::spawn(move || {
            listener.set_nonblocking(true).unwrap();
            
            while running.load(Ordering::SeqCst) {
                match listener.accept() {
                    Ok((mut stream, addr)) => {
                        println!("New connection from: {}", addr);
                        let _ = stream.write_all(RESPONSE);
                    }
                    Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                        thread::sleep(std::time::Duration::from_millis(10));
                        continue;
                    }
                    Err(e) => eprintln!("Accept error: {}", e),
                }
            }
        }));

        Ok(())
    }

    #[napi]
    pub fn stop(&mut self) {
        self.running.store(false, Ordering::SeqCst);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        self.stop();
    }
}
