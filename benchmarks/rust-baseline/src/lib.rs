use spin_sdk::http::{IntoResponse, Method, Request, Response};
use spin_sdk::http_component;

/// Equivalent HTTP handler to SpinHandler.ex for benchmarking.
///
/// Routes:
///   GET  /        -> HTML welcome page
///   GET  /hello   -> plain text greeting
///   POST /echo    -> echo body back
///   GET  /json    -> JSON response
///   GET  /compute -> fibonacci + factorial computation
///   *    *        -> 404
#[http_component]
fn handle(req: Request) -> anyhow::Result<impl IntoResponse> {
    let path = req.path();
    let method = req.method();
    let is_get = *method == Method::Get;

    match (is_get, path) {
        (true, "/") => Ok(Response::builder()
            .status(200)
            .header("content-type", "text/html; charset=utf-8")
            .body(concat!(
                "<html><head><title>Rust on Spin</title></head><body>",
                "<h1>Hello from Rust on Spin!</h1>",
                "<p>Running natively on Fermyon Spin.</p>",
                "<ul>",
                "<li><a href=\"/hello\">/hello</a></li>",
                "<li><a href=\"/json\">/json</a></li>",
                "<li><a href=\"/compute\">/compute</a></li>",
                "<li>POST <a href=\"/echo\">/echo</a></li>",
                "</ul></body></html>",
            ))
            .build()),

        (true, "/hello") => Ok(Response::builder()
            .status(200)
            .header("content-type", "text/plain; charset=utf-8")
            .body("Hello from Rust on Spin!")
            .build()),

        (_, "/echo") => {
            let body = req.into_body();
            Ok(Response::builder()
                .status(200)
                .header("content-type", "application/octet-stream")
                .body(body)
                .build())
        }

        (true, "/json") => Ok(Response::builder()
            .status(200)
            .header("content-type", "application/json")
            .body(r#"{"message":"Hello from Rust on Spin","platform":"wasi","features":["pattern_matching","zero_cost_abstractions","ownership","traits"]}"#)
            .build()),

        (true, "/compute") => {
            let fib_10 = fib(10);
            let fact_12 = factorial(12);
            let squares: Vec<u64> = (1..=10).map(|x| x * x).collect();
            let sum: u64 = squares.iter().sum();
            let evens: Vec<u64> = (1..=10).filter(|x| x % 2 == 0).collect();

            let squares_str = format!("{:?}", squares);
            let evens_str = format!("{:?}", evens);

            let body = format!(
                "Rust computations on WebAssembly:\n\n\
                 \x20 fib(10)     = {fib_10}\n\
                 \x20 factorial(12)= {fact_12}\n\
                 \x20 squares     = {squares_str}\n\
                 \x20 sum(squares)= {sum}\n\
                 \x20 evens(1..10)= {evens_str}\n\
                 \nAll computed in Rust running on Spin/WASM!\n"
            );

            Ok(Response::builder()
                .status(200)
                .header("content-type", "text/plain; charset=utf-8")
                .body(body)
                .build())
        }

        _ => Ok(Response::builder()
            .status(404)
            .header("content-type", "text/plain")
            .body("Not Found")
            .build()),
    }
}

fn fib(n: u64) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => fib(n - 1) + fib(n - 2),
    }
}

fn factorial(n: u64) -> u64 {
    match n {
        0 => 1,
        _ => n * factorial(n - 1),
    }
}
