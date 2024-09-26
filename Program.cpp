#include <iostream>
#include "ArgumentParser.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>

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

    // Kontrola, zda je server IP adresa nebo doménové jméno
    struct hostent *host;
    if ((host = gethostbyname(args.server.c_str())) == NULL)
    {
        // Pokud se nepodařilo získat IP adresu, vypiš chybu
        std::cerr << "Nepodařilo se získat IP adresu serveru" << std::endl;
        return 1;
    }

    std::cout << "IP adresa serveru: " << inet_ntoa(*((struct in_addr *)host->h_addr)) << std::endl;

    // Vytvoření socketu
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        // Pokud se nepodařilo vytvořit socket, vypiš chybu
        std::cerr << "Nepodařilo se vytvořit socket" << std::endl;
        return 1;
    }

    // Nastavení adresy serveru
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(args.port);
    server.sin_addr = *((struct in_addr *)host->h_addr);

    // Připojení k serveru
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        // Pokud se nepodařilo připojit k serveru, vypiš chybu
        std::cerr << "Nepodařilo se připojit k serveru" << std::endl;
        return 1;
    }

    // Vypsání informace o připojení
    std::cout << "Připojení k serveru " << args.server << " na portu " << args.port << std::endl;

    // Uzavření socketu
    close(sock);

    return 0;
}
