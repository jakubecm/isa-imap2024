#include <iostream>
#include "ArgumentParser.h"

int main(int argc, char *argv[])
{
    // Vytvoření instance třídy ArgumentParser a parsování argumentů
    ArgumentParser parser(argc, argv);
    ArgumentParser::ParsedArgs args = parser.parse();

    // Vypiš výsledky parsování pro kontrolu
    std::cout << "Server: " << args.server << std::endl;
    std::cout << "Port: " << (args.port ? args.port : (args.use_tls ? 993 : 143)) << std::endl;
    std::cout << "Použít TLS: " << (args.use_tls ? "Ano" : "Ne") << std::endl;
    std::cout << "Certifikát: " << args.certfile << std::endl;
    std::cout << "Adresář certifikátů: " << args.certaddr << std::endl;
    std::cout << "Nové zprávy: " << (args.new_only ? "Ano" : "Ne") << std::endl;
    std::cout << "Hlavičky: " << (args.headers_only ? "Ano" : "Ne") << std::endl;
    std::cout << "Autentizační soubor: " << args.authfile << std::endl;
    std::cout << "Schránka: " << args.mailbox << std::endl;
    std::cout << "Výstupní adresář: " << args.outdir << std::endl;

    // Zde pokračuj s dalšími funkcemi programu, jako je připojení k IMAP serveru, autentizace apod.

    return 0;
}
