#include <iostream>
#include "ArgumentParser.h"
#include "IMAPClient.cpp"
#include "EmailMessage.cpp"
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

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

    return username + " " + password;
}

void parseIMAPResponse(const std::string &fetchResponse, std::vector<std::string> &rawEmails, std::vector<std::string> &UIDs)
{
    std::istringstream stream(fetchResponse);
    std::string line;
    std::string currentEmail;
    bool readingBody = false;
    int expectedBodySize = 0;
    std::string currentUID;

    while (std::getline(stream, line))
    {
        // Check for the start of a new FETCH response
        if (line.find("FETCH (UID ") != std::string::npos)
        {
            // Store the previous email if we were reading a body
            if (!currentEmail.empty() && readingBody && expectedBodySize == 0)
            {
                rawEmails.push_back(currentEmail);
                currentEmail.clear();
            }

            // Find and extract the UID
            size_t uidStart = line.find("UID ") + 4;
            size_t uidEnd = line.find(" ", uidStart);
            currentUID = line.substr(uidStart, uidEnd - uidStart);
            UIDs.push_back(currentUID);

            // Check for the body size
            size_t bodyStart = line.find("{");
            if (bodyStart != std::string::npos)
            {
                size_t bodyEnd = line.find("}", bodyStart);
                expectedBodySize = std::stoi(line.substr(bodyStart + 1, bodyEnd - bodyStart - 1));
                readingBody = true;
            }
        }
        else if (readingBody)
        {
            currentEmail += line + "\n";
            expectedBodySize -= line.length() + 1;

            if (expectedBodySize <= 0)
            {
                rawEmails.push_back(currentEmail);
                currentEmail.clear();
                readingBody = false;
            }
        }
    }

    // Handle the last email in case it was not added
    if (!currentEmail.empty() && readingBody && expectedBodySize == 0)
    {
        rawEmails.push_back(currentEmail);
    }
}

int main(int argc, char *argv[])
{
    ArgumentParser parser(argc, argv);
    ArgumentParser::ParsedArgs args = parser.parse();
    IMAPClient client(args.use_tls);

    // Connect to the IMAP server
    if (!client.connect(args.server, args.port, 5, args.certfile, args.certaddr))
    {
        std::cerr << "Failed to connect to the server." << std::endl;
        return 1;
    }

    client.sendCommand("LOGIN " + parseLogin(args.authfile));
    client.sendCommand("SELECT " + args.mailbox);

    std::string fetchCommand;
    if (args.new_only)
    {
        std::string searchResponse = client.sendCommand("UID SEARCH UNSEEN");
        std::vector<std::string> unseenUIDs;
        std::istringstream iss(searchResponse);
        std::string word;

        while (iss >> word)
        {
            if (word != "UID" && word != "SEARCH" && word != "OK" && word != "BYE")
            {
                unseenUIDs.push_back(word);
            }
        }

        if (unseenUIDs.empty())
        {
            std::cout << "No unread messages found." << std::endl;
            client.sendCommand("LOGOUT");
            client.disconnect();
            return 0;
        }

        std::string uidList = unseenUIDs[0];
        for (size_t i = 1; i < unseenUIDs.size(); ++i)
        {
            uidList += "," + unseenUIDs[i];
        }

        fetchCommand = args.headers_only ? "UID FETCH " + uidList + " (UID BODY.PEEK[HEADER])" : "UID FETCH " + uidList + " (UID BODY[])";
    }
    else
    {
        fetchCommand = args.headers_only ? "UID FETCH 1:* (UID BODY.PEEK[HEADER])" : "UID FETCH 1:* (UID BODY[])";
    }

    std::string fetchResponse = client.sendCommand(fetchCommand);

    std::vector<std::string> rawEmails;
    std::vector<std::string> UIDs;
    parseIMAPResponse(fetchResponse, rawEmails, UIDs);

    // Process and save emails
    int downloadedCount = 0;
    for (size_t i = 0; i < rawEmails.size(); ++i)
    {
        try
        {
            EmailMessage message;
            message.parseMessage(rawEmails[i]);

            // Save email to file
            message.saveToFile(args.outdir, UIDs[i], args.mailbox);
            ++downloadedCount;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to process email " << i + 1 << ": " << ex.what() << std::endl;
        }
    }

    std::cout << "Number of downloaded messages: " << downloadedCount << std::endl;

    // Logout and disconnect
    client.sendCommand("LOGOUT");
    client.disconnect();

    return 0;
}
