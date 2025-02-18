use napi_derive::napi;
use std::ffi::c_void;

extern "C" {
    fn http_server_init() -> *mut c_void;
    fn http_server_listen(server: *mut c_void, port: i32) -> i32;
    fn http_server_stop(server: *mut c_void);
}

#[napi]
pub struct Server {
    handle: *mut c_void,
}

impl Drop for Server {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { http_server_stop(self.handle); }
        }
    }
}

#[napi]
impl Server {
    #[napi(constructor)]
    pub fn new() -> napi::Result<Self> {
        unsafe {
            let handle = http_server_init();
            if handle.is_null() {
                return Err(napi::Error::from_reason("Failed to initialize server"));
            }
            Ok(Server { handle })
        }
    }

    #[napi]
    pub fn listen(&self, port: i32) -> napi::Result<()> {
        if self.handle.is_null() {
            return Err(napi::Error::from_reason("Server not initialized"));
        }
        let result = unsafe { http_server_listen(self.handle, port) };
        if result < 0 {
            return Err(napi::Error::from_reason("Failed to start server"));
        }
        Ok(())
    }

    #[napi]
    pub fn stop(&self) {
        unsafe { http_server_stop(self.handle) }
    }
}
