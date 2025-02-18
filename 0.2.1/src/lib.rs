use napi_derive::napi;
use std::sync::Arc;
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::Notify;
use tokio::io::{self, AsyncWriteExt};
use tokio::task::JoinHandle;

const RESPONSE: &[u8] = b"HTTP/1.1 200 OK\r\n\
    Content-Length: 13\r\n\
    Content-Type: text/plain\r\n\
    Connection: keep-alive\r\n\
    \r\n\
    Hello, World!\n";

struct ServerState {
    shutdown: Arc<Notify>,
    runtime: tokio::runtime::Runtime,
    server_task: Option<JoinHandle<anyhow::Result<()>>>,
}

#[napi]
pub struct Server {
    state: Option<ServerState>,
}

async fn handle_connection(mut socket: TcpStream) -> io::Result<()> {
    socket.write_all(RESPONSE).await
}

async fn run_server(addr: String, shutdown: Arc<Notify>) -> anyhow::Result<()> {
    let listener = TcpListener::bind(&addr).await
        .map_err(|e| {
            eprintln!("Failed to bind to {}: {}", addr, e);
            e
        })?;
    println!("Server listening on {}", addr);

    loop {
        tokio::select! {
            result = listener.accept() => {
                match result {
                    Ok((socket, addr)) => {
                        println!("New connection from: {}", addr);
                        tokio::spawn(handle_connection(socket));
                    }
                    Err(e) => eprintln!("Accept error: {}", e),
                }
            }
            _ = shutdown.notified() => {
                println!("Shutting down server...");
                break;
            }
        }
    }
    Ok(())
}

#[napi]
impl Server {
    #[napi(constructor)]
    pub fn new() -> napi::Result<Self> {
        println!("Initializing server...");
        let runtime = tokio::runtime::Runtime::new()
            .map_err(|e| {
                eprintln!("Failed to create runtime: {}", e);
                napi::Error::from_reason(e.to_string())
            })?;
        
        Ok(Server {
            state: Some(ServerState {
                shutdown: Arc::new(Notify::new()),
                runtime,
                server_task: None,
            })
        })
    }

    #[napi]
    pub fn listen(&mut self, port: i32) -> napi::Result<()> {
        if let Some(state) = &mut self.state {
            let addr = format!("0.0.0.0:{}", port);
            let shutdown = state.shutdown.clone();
            
            println!("Starting server on {}...", addr);
            
            let handle = state.runtime.spawn(run_server(addr, shutdown));
            state.server_task = Some(handle);

            state.runtime.block_on(async {
                tokio::time::sleep(tokio::time::Duration::from_secs(1)).await;
            });

            Ok(())
        } else {
            Err(napi::Error::from_reason("Server not initialized"))
        }
    }

    #[napi]
    pub fn stop(&mut self) {
        if let Some(state) = &mut self.state {
            println!("Stopping server...");
            state.shutdown.notify_one();
            
            if let Some(task) = state.server_task.take() {
                state.runtime.block_on(async {
                    if let Err(e) = task.await {
                        eprintln!("Error during shutdown: {:?}", e);
                    }
                });
            }
        }
        self.state = None;
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        self.stop();
    }
}