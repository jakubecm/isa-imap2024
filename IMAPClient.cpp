#include <iostream>
#include <iomanip>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <regex>

class IMAPClient
{
private:
    int socket_fd;
    SSL *ssl;
    SSL_CTX *ctx;
    bool use_tls;
    int command_counter;

public:
    IMAPClient(bool use_tls)
        : socket_fd(-1), ssl(nullptr), ctx(nullptr), use_tls(use_tls), command_counter(1) {}

    /**
     * @brief Connect to an IMAP server using regular sockets and optional SSL and read the server greeting.
     *
     * @param server The server address
     * @param port The port number
     * @param timeout The connection timeout in seconds
     * @param certfile The path to the certificate file
     * @param certaddr The path to the certificate store
     * @return true if the connection was successful, false otherwise
     */
    bool connect(const std::string &server, int port, int timeout, const std::string &certfile = "", const std::string &certaddr = "/etc/ssl/certs")
    {
        struct sockaddr_in server_addr;
        struct hostent *host;

        if ((host = gethostbyname(server.c_str())) == nullptr)
        {
            std::cerr << "Error: Failed to resolve hostname." << std::endl;
            return false;
        }

        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0)
        {
            std::cerr << "Error: Failed to create socket." << std::endl;
            return false;
        }

        // Set socket to non-blocking mode for timeout handling
        fcntl(socket_fd, F_SETFL, O_NONBLOCK);

        // Set up server address structure
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

        // Attempt to connect
        int result = ::connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (result < 0 && errno != EINPROGRESS)
        {
            std::cerr << "Error: Connection failed." << std::endl;
            close(socket_fd);
            return false;
        }

        // Wait for the connection to complete or timeout
        fd_set fdset;
        struct timeval tv;
        FD_ZERO(&fdset);
        FD_SET(socket_fd, &fdset);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        result = select(socket_fd + 1, nullptr, &fdset, nullptr, &tv);
        if (result <= 0)
        {
            std::cerr << (result == 0 ? "Connection timed out." : "Connection failed due to select() error.") << std::endl;
            close(socket_fd);
            return false;
        }

        // Set socket back to blocking mode
        fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) & ~O_NONBLOCK);

        if (use_tls)
        {
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            SSL_library_init();

            ctx = SSL_CTX_new(SSLv23_client_method());
            if (!ctx)
            {
                ERR_print_errors_fp(stderr);
                close(socket_fd);
                return false;
            }

            if (!certfile.empty())
            {
                if (!SSL_CTX_load_verify_locations(ctx, certfile.c_str(), certaddr.c_str()))
                {
                    std::cerr << "Error: Failed to load certificates." << std::endl;
                    close(socket_fd);
                    return false;
                }
            }

            ssl = SSL_new(ctx);
            if (!ssl)
            {
                std::cerr << "Failed to create SSL structure." << std::endl;
                close(socket_fd);
                return false;
            }

            SSL_set_fd(ssl, socket_fd);
            if (SSL_connect(ssl) <= 0)
            {
                std::cerr << "SSL/TLS handshake failed." << std::endl;
                close(socket_fd);
                return false;
            }

            std::cout << "Connected securely to " << server << " on port " << port << std::endl;
        }
        else
        {
            std::cout << "Connected non-securely to " << server << " on port " << port << std::endl;
        }

        readResponse("*"); // Read the server greeting
        return true;
    }

    /**
     * @brief Send a command to the server using regular socket or SSL.
     * Read the response from the server afterwards.
     *
     * @param command The command to send
     * @return The response from the server
     */
    std::string sendCommand(const std::string &command)
    {
        std::stringstream numbered_command;
        numbered_command << "A" << std::setw(3) << std::setfill('0') << command_counter++ << " " << command << "\r\n";

        std::string full_command = numbered_command.str();

        if (use_tls)
        {
            SSL_write(ssl, full_command.c_str(), full_command.size());
        }
        else
        {
            send(socket_fd, full_command.c_str(), full_command.size(), 0);
        }

        std::cout << "Sent command: " << command << std::endl;
        std::string response = readResponse(full_command.substr(0, 4)); // Read the response and check the tagged response

        return response; // Return the full response
    }

    /**
     * @brief Read the response from the server. If it is longer than 4096 bytes,
     * keep reading until the whole response is read.
     *
     * @param tag The tag of the command to read the response for
     * @return The response from the server
     */
    std::string readResponse(const std::string &tag)
    {
        char buffer[4096];
        std::string response;
        int bytes_read;

        while (true)
        {
            memset(buffer, 0, sizeof(buffer));

            if (use_tls)
            {
                bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
            }
            else
            {
                bytes_read = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
            }

            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                response += buffer;

                if (tag == "*")
                {
                    if (response.find("* ") == 0)
                    {
                        if (response.find("\r\n") != std::string::npos)
                        {
                            break; // Complete untagged response received
                        }
                    }
                }
                else
                {
                    std::string tag_with_status = tag + " ";
                    if (response.find(tag_with_status) != std::string::npos)
                    {
                        break; // Complete tagged response received
                    }
                }
            }
            else if (bytes_read == 0)
            {
                break; // Connection closed by server
            }
            else if (bytes_read < 0)
            {
                if (use_tls && SSL_get_error(ssl, bytes_read) == SSL_ERROR_WANT_READ)
                {
                    continue; // Retry if needed
                }
                else if (!use_tls && (errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    continue; // Retry non-blocking operation
                }
                else
                {
                    std::cerr << "Error reading from server." << std::endl;
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
        if (use_tls && ssl)
        {
            int shutdown_status = SSL_shutdown(ssl);

            if (shutdown_status == 0)
            {
                shutdown_status = SSL_shutdown(ssl); // Call it again to complete the shutdown
            }

            if (shutdown_status == 1)
            {
                std::cout << "SSL/TLS connection closed gracefully." << std::endl;
            }
            else
            {
                std::cerr << "Error during SSL/TLS shutdown." << std::endl;
                ERR_print_errors_fp(stderr);
            }

            SSL_free(ssl);
            ssl = nullptr;
        }

        if (socket_fd != -1)
        {
            close(socket_fd);
            socket_fd = -1;
        }

        if (ctx)
        {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
};
