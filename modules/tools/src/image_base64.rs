use base64::{engine::general_purpose, Engine as _};
use clap::{Arg, ArgAction, Command};
use image::{imageops::FilterType, ImageFormat, ImageReader};
use log::{error, info};
use std::error::Error;
use std::fs::{read_to_string, File};
use std::io::{Cursor, Read, Write};
use std::path::{Path, PathBuf};

fn encode_image_to_base64(
    image_path: &str,
    format: Option<ImageFormat>,
    quality: Option<u8>,
    resize: Option<(u32, u32)>,
    url_safe: bool,
) -> Result<String, Box<dyn Error>> {
    info!("Starting to encode image: {}", image_path);
    let mut img = ImageReader::open(image_path)?.decode()?;
    info!("Image opened and decoded successfully");

    // Resize the image if needed
    if let Some((width, height)) = resize {
        info!("Resizing image to {}x{}", width, height);
        img = img.resize_exact(width, height, FilterType::Lanczos3);
    }

    let mut buffer = Vec::new();
    let fmt = format.unwrap_or_else(|| {
        Path::new(image_path)
            .extension()
            .and_then(
                |ext| match ext.to_str().unwrap_or("").to_lowercase().as_str() {
                    "jpg" | "jpeg" => Some(ImageFormat::Jpeg),
                    "png" => Some(ImageFormat::Png),
                    "tif" | "tiff" => Some(ImageFormat::Tiff),
                    "webp" => Some(ImageFormat::WebP),
                    "bmp" => Some(ImageFormat::Bmp),
                    _ => Some(ImageFormat::Png),
                },
            )
            .unwrap_or(ImageFormat::Png)
    });

    // Validate quality parameter
    if quality.is_some() && fmt != ImageFormat::Jpeg {
        error!("Quality parameter is only supported for JPEG format");
        return Err("Quality parameter is only supported for JPEG format".into());
    }

    // Save the image to buffer
    match fmt {
        ImageFormat::Jpeg => {
            info!("Saving image as JPEG with quality {:?}", quality);
            img.write_to(&mut Cursor::new(&mut buffer), ImageFormat::Jpeg)?;
        }
        _ => {
            info!("Saving image as {:?}", fmt);
            img.write_to(&mut Cursor::new(&mut buffer), fmt)?;
        }
    }

    // Choose Base64 encoding engine
    let engine = if url_safe {
        info!("Using URL-safe Base64 encoding");
        &general_purpose::URL_SAFE
    } else {
        info!("Using standard Base64 encoding");
        &general_purpose::STANDARD
    };

    let encoded = engine.encode(&buffer);
    info!("Image encoded to Base64 successfully");
    Ok(encoded)
}

fn decode_base64_to_image(
    base64_str: &str,
    output_path: &str,
    url_safe: bool,
) -> Result<(), Box<dyn Error>> {
    info!("Starting to decode Base64 string to image");
    let engine = if url_safe {
        info!("Using URL-safe Base64 decoding");
        &general_purpose::URL_SAFE
    } else {
        info!("Using standard Base64 decoding");
        &general_purpose::STANDARD
    };

    // Decode the Base64 string
    let decoded_data = engine.decode(base64_str.trim())?;
    info!("Base64 string decoded successfully");

    // Open the decoded data as an image
    let img = ImageReader::new(Cursor::new(decoded_data))
        .with_guessed_format()?
        .decode()?;
    info!("Image data decoded successfully");

    // Synchronously create and write to the output file
    let mut output_file = File::create(output_path)?;
    img.write_to(&mut output_file, ImageFormat::Png)?;
    info!("Image saved to {}", output_path);

    Ok(())
}

fn encode_multiple_images(
    image_paths: Vec<String>,
    format: Option<ImageFormat>,
    quality: Option<u8>,
    resize: Option<(u32, u32)>,
    url_safe: bool,
    output_file: Option<&str>,
) -> Result<(), Box<dyn Error>> {
    info!("Starting batch encoding of images");
    let mut results = Vec::new();

    for path in image_paths {
        info!("Encoding image: {}", path);
        let encoded = encode_image_to_base64(&path, format, quality, resize, url_safe)?;
        results.push((path, encoded));
    }

    if let Some(output_path) = output_file {
        info!("Writing encoded results to file: {}", output_path);
        let mut file = File::create(output_path)?;
        for (path, encoded) in results {
            writeln!(file, "{}: {}", path, encoded)?;
        }
    } else {
        for (path, encoded) in results {
            println!("{}: {}", path, encoded);
        }
    }

    info!("Batch encoding completed successfully");
    Ok(())
}

fn decode_from_file(
    base64_file: &str,
    output_dir: &str,
    url_safe: bool,
) -> Result<PathBuf, Box<dyn Error>> {
    info!("Starting to decode Base64 from file: {}", base64_file);
    let base64_str = read_to_string(base64_file)?;
    let output_path = Path::new(output_dir).join("decoded_image.png");

    decode_base64_to_image(&base64_str, output_path.to_str().unwrap(), url_safe)?;
    info!("Decoded image saved to {}", output_path.display());

    Ok(output_path)
}

fn main() -> Result<(), Box<dyn Error>> {
    env_logger::init();
    info!("Starting image-base64 utility");

    let matches = Command::new("image-base64")
        .version("2.1")
        .author("Max Qian <astro_air@126.com>")
        .about("Advanced image Base64 encoding/decoding tool")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(
            Command::new("encode")
                .about("Encode an image to Base64")
                .arg(
                    Arg::new("image")
                        .help("Path to the image to encode")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("format")
                        .help("Output format (jpeg, png, webp, bmp)")
                        .long("format")
                        .short('f')
                        .value_parser(["jpeg", "png", "webp", "bmp"]),
                )
                .arg(
                    Arg::new("quality")
                        .help("JPEG quality (1-100)")
                        .long("quality")
                        .short('q')
                        .value_parser(1..=100),
                )
                .arg(
                    Arg::new("resize")
                        .help("Resize image to WxH pixels")
                        .long("resize")
                        .short('s')
                        .value_parser(clap::value_parser!(u32))
                        .number_of_values(2),
                )
                .arg(
                    Arg::new("url-safe")
                        .help("Use URL-safe Base64 encoding")
                        .long("url-safe")
                        .action(ArgAction::SetTrue),
                )
                .arg(
                    Arg::new("output")
                        .help("Output file for Base64 string")
                        .long("output")
                        .short('o'),
                ),
        )
        .subcommand(
            Command::new("decode")
                .about("Decode Base64 to image")
                .arg(
                    Arg::new("base64")
                        .help("Base64 string or '-' to read from stdin")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("output")
                        .help("Output image path")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("url-safe")
                        .help("Use URL-safe Base64 decoding")
                        .long("url-safe")
                        .action(ArgAction::SetTrue),
                ),
        )
        .subcommand(
            Command::new("decode-from-file")
                .about("Decode Base64 from file")
                .arg(
                    Arg::new("input")
                        .help("Input file containing Base64 string")
                        .required(true)
                        .index(1),
                )
                .arg(
                    Arg::new("output-dir")
                        .help("Output directory for decoded image")
                        .required(true)
                        .index(2),
                )
                .arg(
                    Arg::new("url-safe")
                        .help("Use URL-safe Base64")
                        .long("url-safe")
                        .action(ArgAction::SetTrue),
                ),
        )
        .subcommand(
            Command::new("batch-encode")
                .about("Batch encode multiple images")
                .arg(
                    Arg::new("images")
                        .help("Comma-separated list of image paths")
                        .required(true)
                        .value_delimiter(','),
                )
                .arg(
                    Arg::new("output")
                        .help("Output file for results")
                        .long("output")
                        .short('o'),
                )
                .arg(
                    Arg::new("format")
                        .help("Output format for all images")
                        .long("format")
                        .short('f'),
                )
                .arg(
                    Arg::new("quality")
                        .help("JPEG quality for all images")
                        .long("quality")
                        .short('q'),
                )
                .arg(
                    Arg::new("resize")
                        .help("Resize all images to WxH")
                        .long("resize")
                        .short('s')
                        .value_parser(clap::value_parser!(u32))
                        .number_of_values(2),
                )
                .arg(
                    Arg::new("url-safe")
                        .help("Use URL-safe Base64")
                        .long("url-safe")
                        .action(ArgAction::SetTrue),
                ),
        )
        .subcommand(
            Command::new("batch-decode")
                .about("Decode multiple Base64 strings from file")
                .arg(
                    Arg::new("input")
                        .help("Input file containing Base64 strings")
                        .required(true),
                )
                .arg(
                    Arg::new("output-dir")
                        .help("Output directory for decoded images")
                        .required(true),
                )
                .arg(
                    Arg::new("url-safe")
                        .help("Use URL-safe Base64")
                        .long("url-safe")
                        .action(ArgAction::SetTrue),
                ),
        )
        .get_matches();

    match matches.subcommand() {
        Some(("encode", sub_matches)) => {
            let image_path = sub_matches.get_one::<String>("image").unwrap();
            let format = sub_matches
                .get_one::<String>("format")
                .map(|f| match f.as_str() {
                    "jpeg" => ImageFormat::Jpeg,
                    "png" => ImageFormat::Png,
                    "webp" => ImageFormat::WebP,
                    "bmp" => ImageFormat::Bmp,
                    _ => unreachable!(),
                });
            let quality = sub_matches.get_one::<u8>("quality").copied();
            let resize = sub_matches
                .get_many::<u32>("resize")
                .map(|mut vals| (*vals.next().unwrap(), *vals.next().unwrap()));
            let url_safe = sub_matches.get_flag("url-safe");
            let output = sub_matches.get_one::<String>("output");

            info!("Encoding image: {}", image_path);
            let base64_str = encode_image_to_base64(image_path, format, quality, resize, url_safe)?;

            if let Some(output_path) = output {
                std::fs::write(output_path, base64_str)?;
                println!("Encoded image saved to {}", output_path);
                info!("Encoded image saved to {}", output_path);
            } else {
                println!("{}", base64_str);
                info!("Encoded image output to stdout");
            }
        }

        Some(("decode", sub_matches)) => {
            let base64_input = sub_matches.get_one::<String>("base64").unwrap();
            let output_path = sub_matches.get_one::<String>("output").unwrap();
            let url_safe = sub_matches.get_flag("url-safe");

            let base64_str = if base64_input == "-" {
                let mut buffer = String::new();
                std::io::stdin().read_to_string(&mut buffer)?;
                buffer
            } else {
                base64_input.clone()
            };

            info!("Decoding Base64 string to image: {}", output_path);
            decode_base64_to_image(&base64_str, output_path, url_safe)?;
            println!("Successfully decoded image to {}", output_path);
            info!("Successfully decoded image to {}", output_path);
        }

        Some(("decode-from-file", sub_matches)) => {
            let input_file = sub_matches.get_one::<String>("input").unwrap();
            let output_dir = sub_matches.get_one::<String>("output-dir").unwrap();
            let url_safe = sub_matches.get_flag("url-safe");

            info!("Decoding Base64 from file: {}", input_file);
            let output_path = decode_from_file(input_file, output_dir, url_safe)?;
            println!("Decoded image saved to {}", output_path.to_str().unwrap());
            info!("Decoded image saved to {}", output_path.to_str().unwrap());
        }

        Some(("batch-encode", sub_matches)) => {
            let images = sub_matches.get_many::<String>("images").unwrap();
            let output = sub_matches.get_one::<String>("output");
            let format = sub_matches
                .get_one::<String>("format")
                .map(|f| match f.as_str() {
                    "jpeg" => ImageFormat::Jpeg,
                    "tif" | "tiff" => ImageFormat::Tiff,
                    "png" => ImageFormat::Png,
                    "webp" => ImageFormat::WebP,
                    "bmp" => ImageFormat::Bmp,
                    _ => unreachable!(),
                });
            let quality = sub_matches.get_one::<u8>("quality").copied();
            let resize = sub_matches
                .get_many::<u32>("resize")
                .map(|mut vals| (*vals.next().unwrap(), *vals.next().unwrap()));
            let url_safe = sub_matches.get_flag("url-safe");

            info!("Batch encoding images");
            encode_multiple_images(
                images.cloned().collect(),
                format,
                quality,
                resize,
                url_safe,
                output.map(String::as_str),
            )?;
        }

        Some(("batch-decode", sub_matches)) => {
            let input_file = sub_matches.get_one::<String>("input").unwrap();
            let output_dir = sub_matches.get_one::<String>("output-dir").unwrap();
            let url_safe = sub_matches.get_flag("url-safe");

            info!("Batch decoding Base64 strings from file: {}", input_file);
            let content = read_to_string(input_file)?;
            for (i, line) in content.lines().enumerate() {
                let output_path = format!("{}/image_{}.png", output_dir, i);
                decode_base64_to_image(line, &output_path, url_safe)?;
                println!("Decoded image {}", output_path);
                info!("Decoded image {}", output_path);
            }
        }

        _ => unreachable!(),
    }

    info!("image-base64 utility finished");
    Ok(())
}
