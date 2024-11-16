# ISA-IMAP2024 Project

**Name:** Milan Jakubec  
**Login:** xjakub41

## Project Description

This project is part of the ISA-IMAP2024 course. It aims to implement an IMAP (Internet Message Access Protocol) client that can interact with an email server to retrieve emails from. It can retreive either headers or full emails, new emails or all emails. It basically synchronizes the email client with the email server. 

## Files

- `ArgumentParser.cpp`: Implementation of the ArgumentParser class for parsing command line arguments.
- `ArgumentParser.h`: A headerfile for class to handle command-line argument parsing for the application.
- `EmailMessage.cpp`: A file implementing a helper mail message class to parse email messages.
- `Helpers.cpp`: A file implementing a class for helper functions that are used.
- `IMAPClient.cpp`: A file implementing an abstraction for an IMAP client using both non-TLS and TLS versions.
- `Makefile`: Build script to compile the project.
- `README.md`: This file, providing an overview of the project.
- `LICENSE` : The license

## How to Build

To build the project, run the following command in the project directory:

```sh
make
```

## How to Run

After building the project, you can run the IMAP client with:

```sh
./imapcl server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir
```

The parameters for the program are as follows:

- `server`: The IMAP server address (either hostname or IP)
- `-p port`: (Optional) The port number to connect to the server.
- `-T`: (Optional) Use TLS for the connection.
- `-c certfile`: (Optional) The certificate file for TLS.
- `-C certaddr`: (Optional) The certificate store for TLS.
- `-n`: (Optional) Retrieve only new emails.
- `-h`: (Optional) Retrieve only headers.
- `-a auth_file`: The authentication file containing login credentials.
- `-b MAILBOX`: (Optional) The mailbox to retrieve emails from.
- `-o out_dir`: The output directory to save the retrieved emails.

## Example of running

```sh
./imapcl imap.pobox.sk -T -a pobox_authfile -o maildir
```

## License

This project is licensed under the MIT License. See the `LICENSE` file for more details.
