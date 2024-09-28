#include <iostream>
#include <iomanip>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <sstream>

class IMAPClient
{
private:
    BIO *bio;
    SSL *ssl;
    SSL_CTX *ctx;
    bool use_tls;
    int command_counter;

public:
    IMAPClient(bool use_tls)
        : bio(nullptr), ssl(nullptr), ctx(nullptr), use_tls(use_tls), command_counter(1) {}

    /**
     * @brief Connect to an IMAP server
     *
     * @param server The server address
     * @param port The port number
     * @param certfile The path to the certificate file
     * @param certaddr The path to the certificate store
     * @return true if the connection was successful, false otherwise
     */
    bool connect(const std::string &server, int port, const std::string &certfile = "", const std::string &certaddr = "/etc/ssl/certs")
    {
        std::string server_port = server + ":" + std::to_string(port);

        if (use_tls)
        {
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            SSL_library_init();

            ctx = SSL_CTX_new(SSLv23_client_method());
            if (!ctx)
            {
                ERR_print_errors_fp(stderr);
                return false;
            }

            if (!certfile.empty())
            {
                if (!SSL_CTX_load_verify_locations(ctx, certfile.c_str(), certaddr.c_str()))
                {
                    ERR_print_errors_fp(stderr);
                    return false;
                }
            }
            else
            {
                if (!SSL_CTX_load_verify_locations(ctx, nullptr, certaddr.c_str()))
                {
                    ERR_print_errors_fp(stderr);
                    return false;
                }
            }

            bio = BIO_new_ssl_connect(ctx);
            if (!bio)
            {
                ERR_print_errors_fp(stderr);
                return false;
            }

            BIO_get_ssl(bio, &ssl);
            BIO_set_conn_hostname(bio, server_port.c_str());

            if (BIO_do_connect(bio) <= 0)
            {
                ERR_print_errors_fp(stderr);
                return false;
            }

            std::cout << "Connected securely to " << server << " on port " << port << std::endl;
        }
        else
        {
            bio = BIO_new_connect(server_port.c_str());
            if (!bio)
            {
                ERR_print_errors_fp(stderr);
                return false;
            }

            if (BIO_do_connect(bio) <= 0)
            {
                ERR_print_errors_fp(stderr);
                return false;
            }

            std::cout << "Connected non-securely to " << server << " on port " << port << std::endl;
        }

        return true;
    }

    /**
     * @brief Send a command to the server, the command is automatically numbered (e.g., A001)
     *
     * @param command The command to send
     * @return The full command sent to the server
     */
    std::string sendCommand(const std::string &command)
    {
        std::stringstream numbered_command;
        numbered_command << "A" << std::setw(3) << std::setfill('0') << command_counter++ << " " << command << "\r\n";

        std::string full_command = numbered_command.str();
        BIO_write(bio, full_command.c_str(), full_command.size());

        return full_command; // Return the sent command for tracking/debugging
    }

    /**
     * @brief Read the response from the server. If it is longer than 4096 bytes,
     * keep reading until the whole response is read.
     *
     * @return The response from the server
     */
    std::string readResponse()
    {
        char buffer[4096];
        std::string response;
        int bytes_read;

        // Read the first chunk of the response
        bytes_read = BIO_read(bio, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            response = buffer;
        }
        else
        {
            // If no data was read or an error occurred, handle it here
            if (bytes_read <= 0 && !BIO_should_retry(bio))
            {
                ERR_print_errors_fp(stderr);
                return "";
            }
        }

        // Keep appending to the response as long as more data is available
        while (BIO_pending(bio) > 0)
        {
            bytes_read = BIO_read(bio, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                response += buffer;
            }
            else
            {
                // Break out if an error occurs
                if (bytes_read <= 0 && !BIO_should_retry(bio))
                {
                    ERR_print_errors_fp(stderr);
                    break;
                }
            }
        }

        return response;
    }

    /**
     * @brief Gracefully disconnect from the server and free up resources.
     */
    void disconnect()
    {
        if (bio)
        {
            BIO_free_all(bio); // Free the BIO chain (including the underlying socket)
            bio = nullptr;
        }

        if (ctx)
        {
            SSL_CTX_free(ctx); // Free the SSL context
            ctx = nullptr;
        }
    }
};
