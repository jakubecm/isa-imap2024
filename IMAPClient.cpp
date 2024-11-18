/**
 * @file IMAPClient.cpp
 * @author Milan Jakubec (xjakub41)
 * @date 2024-11-15
 * @brief A file implementing an abstraction for an IMAP client using OpenSSL.
 */

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

/**
 * @brief An IMAP client class that can connect to an IMAP server using regular sockets or SSL.
 */
class IMAPClient
{
private:
    int socket_fd;       // Socket file descriptor
    SSL *ssl;            // SSL structure
    SSL_CTX *ctx;        // SSL context
    bool use_tls;        // Whether to use SSL/TLS
    int command_counter; // Counter for IMAP commands (tagged)

public:
    std::string canonical_hostname; // The canonical hostname of the server (meaning the fully qualified domain name)

    /**
     * @brief Construct a new IMAPClient object.
     * Sets socket_fd to -1 and initializes the SSL context and structure to nullptr.
     */
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
    bool connect(const std::string &server, int port, int timeout, const std::string &certfile, const std::string &certaddr)
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

        // Set send and receive timeout on the socket using setsockopt
        struct timeval timeout_val;
        timeout_val.tv_sec = timeout;
        timeout_val.tv_usec = 0;

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
            std::cerr << "Error: Connection failed right away." << std::endl;
            close(socket_fd);
            return false;
        }

        // Wait for the connection to complete or timeout
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(socket_fd, &fdset);

        result = select(socket_fd + 1, nullptr, &fdset, nullptr, &timeout_val);
        if (result <= 0)
        {
            std::cerr << (result == 0 ? "Connection timed out." : "Connection failed due to select() error.") << std::endl;
            close(socket_fd);
            return false;
        }

        // Set socket back to blocking mode
        fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) & ~O_NONBLOCK);

        // Get the canonical hostname of the server
        char hostname[NI_MAXHOST];
        result = getnameinfo((struct sockaddr *)&server_addr, sizeof(server_addr), hostname, NI_MAXHOST, NULL, 0, 0);
        if (result != 0)
        {
            std::cerr << "Error: Failed to get canonical hostname: " << gai_strerror(result) << std::endl;
            canonical_hostname = server;
        }
        else
        {
            canonical_hostname = std::string(hostname);
        }

        if (use_tls)
        {
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            SSL_library_init();

            ctx = SSL_CTX_new(TLS_client_method());
            if (!ctx)
            {
                ERR_print_errors_fp(stderr);
                close(socket_fd);
                return false;
            }

            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

            if (!SSL_CTX_load_verify_locations(ctx, certfile.empty() ? nullptr : certfile.c_str(), certaddr.c_str()))
            {
                std::cerr << "Error: Failed to load certificates." << std::endl;
                close(socket_fd);
                return false;
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

        // Set up the timeout structure
        struct timeval timeout_val;
        timeout_val.tv_sec = 5;
        timeout_val.tv_usec = 0;

        fd_set read_fds;

        while (true)
        {
            memset(buffer, 0, sizeof(buffer));

            // Prepare the file descriptor set
            FD_ZERO(&read_fds);
            FD_SET(socket_fd, &read_fds);

            // Wait for data to be available for reading
            int result = select(socket_fd + 1, &read_fds, nullptr, nullptr, &timeout_val);

            if (result > 0)
            {
                // Data is available, proceed to read
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

                    // Check if the response contains the expected tag
                    if (tag == "*")
                    {
                        if (response.find("* ") == 0 && response.find("\r\n") != std::string::npos)
                        {
                            break; // Complete untagged response received
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
                    std::cerr << "Error: Connection closed by server." << std::endl;
                    disconnect();
                    exit(EXIT_FAILURE);
                }
                else if (bytes_read < 0)
                {
                    if (use_tls && SSL_get_error(ssl, bytes_read) == SSL_ERROR_WANT_READ)
                    {
                        continue; // Retry if needed
                    }
                    else
                    {
                        std::cerr << "Error reading from server." << std::endl;
                        disconnect();
                        exit(EXIT_FAILURE);
                    }
                }
            }
            else if (result == 0)
            {
                std::cerr << "Error: Read operation timed out." << std::endl;
                disconnect();
                exit(EXIT_FAILURE);
            }
            else
            {
                std::cerr << "Error: select() failed." << std::endl;
                disconnect();
                exit(EXIT_FAILURE);
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
                return;
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
