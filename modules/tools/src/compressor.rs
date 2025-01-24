use clap::{Arg, Command};
use flate2::read::GzDecoder;
use flate2::write::GzEncoder;
use flate2::Compression;
use indicatif::{ProgressBar, ProgressStyle};
use std::fs::{self, File};
use std::io::{self, Write};
use std::path::Path;
use walkdir::WalkDir;

/// Main entry point for the compression tool.
/// Handles command line argument parsing and dispatches to appropriate functions.
fn main() {
    let matches = Command::new("rcomp")
        .version("1.0")
        .author("Your Name <youremail@example.com>")
        .about("File compression and decompression tool")
        .subcommand(
            Command::new("compress")
                .about("Compress a file or directory")
                .arg(
                    Arg::new("INPUT")
                        .help("Input file/directory to compress")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("OUTPUT")
                        .help("Output compressed file")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("level")
                        .short('l')
                        .long("level")
                        .help("Compression level (1-9)")
                        .value_parser(1..=9)
                        .default_value("6"),
                ),
        )
        .subcommand(
            Command::new("decompress")
                .about("Decompress a file")
                .arg(
                    Arg::new("INPUT")
                        .help("Input file to decompress")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("OUTPUT")
                        .help("Output directory")
                        .required(true)
                        .index(2),
                ),
        )
        .get_matches();

    println!("Starting rcomp utility...");

    if let Some(matches) = matches.subcommand_matches("compress") {
        let input = matches.get_one::<String>("INPUT").unwrap();
        let output = matches.get_one::<String>("OUTPUT").unwrap();
        let level = matches.get_one::<u32>("level").unwrap();

        println!(
            "Compressing '{}' to '{}' with level {}",
            input, output, level
        );
        if let Err(e) = compress_path(input, output, *level) {
            eprintln!("Compression failed: {}", e);
        }
    } else if let Some(matches) = matches.subcommand_matches("decompress") {
        let input = matches.get_one::<String>("INPUT").unwrap();
        let output = matches.get_one::<String>("OUTPUT").unwrap();

        println!("Decompressing '{}' to '{}'", input, output);
        if let Err(e) = decompress_file(input, output) {
            eprintln!("Decompression failed: {}", e);
        }
    }
}

/// Compresses a file or directory based on the input path.
///
/// # Arguments
/// * `input` - Path to the input file or directory
/// * `output` - Path where the compressed file will be saved
/// * `level` - Compression level (1-9)
fn compress_path(input: &str, output: &str, level: u32) -> io::Result<()> {
    let input_path = Path::new(input);
    println!("Analyzing input path: {}", input);

    if input_path.is_dir() {
        println!("Input is a directory, using tar+gz compression");
        compress_dir(input, output, level)
    } else {
        println!("Input is a file, using gz compression");
        compress_file(input, output, level)
    }
}

/// Compresses a single file using gzip compression.
///
/// # Arguments
/// * `input` - Path to the input file
/// * `output` - Path where the compressed file will be saved
/// * `level` - Compression level (1-9)
fn compress_file(input: &str, output: &str, level: u32) -> io::Result<()> {
    println!("Opening input file: {}", input);
    let input_file = File::open(input)?;
    let input_size = input_file.metadata()?.len();

    let pb = ProgressBar::new(input_size);
    // 修复模板错误处理
    let style = ProgressStyle::default_bar()
        .template("[{elapsed_precise}] [{bar:40.cyan/blue}] {bytes}/{total_bytes} ({eta})")
        .unwrap();
    pb.set_style(style);

    println!("Creating output file: {}", output);
    let output_file = File::create(output)?;
    let mut encoder = GzEncoder::new(output_file, Compression::new(level));

    println!("Starting compression process...");
    let mut reader = io::BufReader::new(input_file);
    io::copy(&mut reader, &mut encoder)?;

    let output_size = fs::metadata(output)?.len();
    pb.finish_with_message(format!(
        "Compression complete! Original: {} bytes, Compressed: {} bytes, Ratio: {:.1}%",
        input_size,
        output_size,
        (1.0 - output_size as f64 / input_size as f64) * 100.0
    ));

    Ok(())
}

/// Compresses a directory using tar+gzip compression.
///
/// # Arguments
/// * `input` - Path to the input directory
/// * `output` - Path where the compressed file will be saved
/// * `level` - Compression level (1-9)
fn compress_dir(input: &str, output: &str, level: u32) -> io::Result<()> {
    println!("Creating tar archive from directory: {}", input);
    let mut archive = tar::Builder::new(Vec::new());

    for entry in WalkDir::new(input).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        if path.is_file() {
            println!("Adding file to archive: {}", path.display());
            archive.append_file(path.strip_prefix(input).unwrap(), &mut File::open(path)?)?;
        }
    }

    println!("Compressing tar archive...");
    let tar_bytes = archive.into_inner()?;
    let output_file = File::create(output)?;
    let mut encoder = GzEncoder::new(output_file, Compression::new(level));
    encoder.write_all(&tar_bytes)?;

    println!("Directory compression complete: {}", output);
    Ok(())
}

/// Decompresses a file or archive.
/// Supports both .gz and .tar.gz/.tgz formats.
///
/// # Arguments
/// * `input` - Path to the compressed file
/// * `output` - Path where files will be extracted
fn decompress_file(input: &str, output: &str) -> io::Result<()> {
    println!("Opening compressed file: {}", input);
    let input_file = File::open(input)?;
    let input_size = input_file.metadata()?.len();

    let pb = ProgressBar::new(input_size);
    let style = ProgressStyle::default_bar()
        .template("[{elapsed_precise}] [{bar:40.cyan/blue}] {bytes}/{total_bytes} ({eta})")
        .unwrap();
    pb.set_style(style);

    let decoder = GzDecoder::new(input_file);

    if input.ends_with(".tar.gz") || input.ends_with(".tgz") {
        println!("Detected tar.gz format, extracting archive...");
        let mut archive = tar::Archive::new(decoder);
        fs::create_dir_all(output)?;
        archive.unpack(output)?;
    } else {
        println!("Detected gz format, decompressing file...");
        let mut output_file = File::create(output)?;
        io::copy(&mut pb.wrap_read(decoder), &mut output_file)?;
    }

    pb.finish_with_message("Decompression complete!");
    Ok(())
}
