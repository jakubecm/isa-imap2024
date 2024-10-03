#include <iostream>
#include "ArgumentParser.h"
#include "IMAPClient.cpp"
#include "EmailMessage.cpp"
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

std::string parseLogin(const std::string &authfile)
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

    return username + " " + password; // Added space between username and password for LOGIN command
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

    bool successfulConnection = client.connect(args.server, args.port, 5, args.certfile, args.certaddr);
    if (!successfulConnection)
    {
        std::cerr << "Failed to connect to the server." << std::endl;
        return 1;
    }

    client.sendCommand("LOGIN " + parseLogin(args.authfile));
    client.sendCommand("SELECT " + args.mailbox);

    // TODO: Set fetch command based on the arguments
    std::string fetchResponse = client.sendCommand("UID FETCH 1:3 (UID BODY[])");
    std::cout << fetchResponse << std::endl;

    // Parse the fetched response into individual email messages
    std::vector<std::string> rawEmails;
    std::vector<std::string> UIDs;
    std::string delimiter = "* "; // Delimiter for individual messages
    size_t pos = 0;

    // Split the response into sections for each email
    while ((pos = fetchResponse.find(delimiter)) != std::string::npos)
    {
        std::string section = fetchResponse.substr(0, pos);
        fetchResponse.erase(0, pos + delimiter.length());

        // Extract the UID of the email
        size_t uidStart = section.find("UID ");
        if (uidStart != std::string::npos)
        {
            uidStart += 4; // Skip the "UID " part
            size_t uidEnd = section.find(" ", uidStart);
            std::string uid = section.substr(uidStart, uidEnd - uidStart);
            UIDs.push_back(uid);
        }

        // Check if the section contains an actual email body (look for {bodySize} format)
        size_t bodyStart = section.find("{");
        if (bodyStart != std::string::npos)
        {
            // Extract the body of the email based on the size given by the server
            size_t bodyEnd = section.find("}", bodyStart);
            if (bodyEnd != std::string::npos)
            {
                int bodySize = std::stoi(section.substr(bodyStart + 1, bodyEnd - bodyStart - 1));
                std::string emailBody = section.substr(bodyEnd + 1, bodySize);
                rawEmails.push_back(emailBody); // Save the parsed email body
            }
        }
    }

    // Ensure the last part of the response is processed (if any)
    if (!fetchResponse.empty())
    {
        size_t bodyStart = fetchResponse.find("{");
        if (bodyStart != std::string::npos)
        {
            size_t bodyEnd = fetchResponse.find("}", bodyStart);
            if (bodyEnd != std::string::npos)
            {
                int bodySize = std::stoi(fetchResponse.substr(bodyStart + 1, bodyEnd - bodyStart - 1));
                std::string emailBody = fetchResponse.substr(bodyEnd + 1, bodySize);
                rawEmails.push_back(emailBody);
            }
        }
    }

    // Process and save emails
    int downloadedCount = 0;
    for (size_t i = 0; i < rawEmails.size(); ++i)
    {
        try
        {
            EmailMessage message;
            message.parse(rawEmails[i]);

            // Save email to file
            message.saveToFile(args.outdir, UIDs[i]);
            ++downloadedCount;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to process email " << i + 1 << ": " << ex.what() << std::endl;
        }
    }

    // Print the number of downloaded messages
    std::cout << "Number of downloaded messages: " << downloadedCount << std::endl;

    // Logout and disconnect
    client.sendCommand("LOGOUT");
    client.disconnect();

    return 0;
}
