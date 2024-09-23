#ifndef ARGUMENTPARSER_H
#define ARGUMENTPARSER_H

#include <string>

class ArgumentParser
{
public:
    // Struktura pro uložení parametrů
    struct ParsedArgs
    {
        std::string server;
        int port = 0; // výchozí port
        bool use_tls = false;
        std::string certfile;
        std::string certaddr = "/etc/ssl/certs"; // výchozí hodnota
        bool new_only = false;
        bool headers_only = false;
        std::string authfile;
        std::string mailbox = "INBOX"; // výchozí schránka
        std::string outdir;
    };

    // Konstruktor
    ArgumentParser(int argc, char *argv[]);

    // Funkce pro parsování argumentů
    ParsedArgs parse();

private:
    int argc;
    char **argv;

    void print_usage();
};

#endif
