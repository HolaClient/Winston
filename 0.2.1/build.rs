use std::process::Command;
use std::env;

fn main() {
    napi_build::setup();
    
    let out_dir = env::var("OUT_DIR").unwrap();
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    
    let format = if target_os == "windows" {
        "win64"
    } else {
        "elf64"
    };
    
    let obj_file = format!("{}/server.o", out_dir);
    let lib_file = format!("{}/libserver.a", out_dir);

    let status = Command::new("nasm")
        .args(&[
            "-f", format,
            "-o", &obj_file,
            "src/server.asm"
        ])
        .status()
        .expect("Failed to execute NASM");

    if !status.success() {
        panic!("NASM compilation failed");
    }

    let ar_status = Command::new("ar")
        .args(&["crs", &lib_file, &obj_file])
        .status()
        .expect("Failed to execute ar");

    if !ar_status.success() {
        panic!("Failed to create static library");
    }

    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=static=server");
    println!("cargo:rerun-if-changed=src/server.asm");
}