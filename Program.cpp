#include <iostream>
#include "ArgumentParser.h"
#include "IMAPClient.cpp"
#include "EmailMessage.cpp"
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>

std::string parseLogin(std::string authfile)
{
    std::ifstream file(authfile);
    std::string line;
    std::string username;
    std::string password;

    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            if (line.find("username") != std::string::npos)
            {
                username = line.substr(line.find("=") + 1);
            }
            else if (line.find("password") != std::string::npos)
            {
                password = line.substr(line.find("=") + 1);
            }
        }
        file.close();
    }

    return username + password;
}

int main(int argc, char *argv[])
{
    ArgumentParser parser(argc, argv);
    ArgumentParser::ParsedArgs args = parser.parse();

    IMAPClient client(args.use_tls);

    // Print out parsed arguments
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

    client.connect(args.server, args.port, args.certfile, args.certaddr);
    std::cout << client.readResponse() << std::endl; // Read the server greeting
    client.sendCommand("LOGIN " + parseLogin(args.authfile));
    std::cout << client.readResponse() << std::endl; // Read the response to the login command
    client.sendCommand("SELECT " + args.mailbox);
    std::cout << client.readResponse() << std::endl; // Read the response to the select command
    // Fetch one full email
    client.sendCommand("FETCH 1 BODY[]");
    std::cout << client.readResponse() << std::endl; // Read the response to the fetch command
    client.sendCommand("LOGOUT");
    std::cout << client.readResponse() << std::endl; // Read the response to the logout command
    client.disconnect();

    return 0;
}
