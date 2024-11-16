/**
 * @file ArgumentParser.h
 * @author Milan Jakubec (xjakub41)
 * @date 2024-11-15
 * @brief Header file for the ArgumentParser class, describing its structures and functions.
 */

#ifndef ARGUMENTPARSER_H
#define ARGUMENTPARSER_H

#include <string>

/**
 * @class ArgumentParser
 * @brief A class to handle command-line argument parsing for the application.
 */
class ArgumentParser
{
public:
    /**
     * @struct ParsedArgs
     * @brief A structure containing parsed command-line arguments.
     */
    struct ParsedArgs
    {
        std::string server;                      /**< Server address or hostname. */
        int port = 0;                            /**< Port number. Defaults to zero. */
        bool use_tls = false;                    /**< Whether to use TLS. Defaults to false. */
        std::string certfile;                    /**< Name of the certificate file */
        std::string certaddr = "/etc/ssl/certs"; /**< Address of the certificate */
        bool new_only = false;                   /**< Whether to download only new messages. Defaults to false. */
        bool headers_only = false;               /**< Whether to download only headers. Defaults to false. */
        std::string authfile;                    /**< Path to the file containing login credentials. */
        std::string mailbox = "INBOX";           /**< Mailbox to download from. Defaults to INBOX. */
        std::string outdir;                      /**< Output directory for the downloaded messages. */
    };

    /**
     * @brief Constructs an ArgumentParser object.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments.
     */
    ArgumentParser(int argc, char *argv[]);

    /**
     * @brief Parses the command-line arguments.
     * @return A ParsedArgs structure with the parsed arguments.
     */
    ParsedArgs parse();

private:
    int argc;
    char **argv;

    /**
     * @brief Prints usage instructions for the application.
     */
    void print_usage();
};

#endif
