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

int main(int argc, char *argv[])
{
    ArgumentParser parser(argc, argv);
    ArgumentParser::ParsedArgs args = parser.parse();
    IMAPClient client(args.use_tls);

    std::string loginCommand = "LOGIN " + Helpers::parseLogin(args.authfile);

    if (!client.connect(args.server, args.port, 5, args.certfile, args.certaddr))
    {
        return EXIT_FAILURE;
    }

    std::string loginResponse = client.sendCommand(loginCommand);

    if (!Helpers::HandleLoginResponse(loginResponse))
    {
        client.disconnect();
        return EXIT_FAILURE;
    }

    std::string selectResponse = client.sendCommand("SELECT " + args.mailbox);

    if (!Helpers::HandleUIDValidity(args.mailbox, args.outdir, selectResponse, client.canonical_hostname))
    {
        client.sendCommand("LOGOUT");
        client.disconnect();
        return EXIT_FAILURE;
    }

    std::string fetchCommand;
    if (args.new_only)
    {
        std::string searchResponse = client.sendCommand("UID SEARCH NEW");
        std::vector<std::string> unseenUIDs;
        std::istringstream iss(searchResponse);
        std::string word;

        // Check for errors in the search response
        if (searchResponse.find("NO") != std::string::npos || searchResponse.find("BAD") != std::string::npos)
        {
            std::cerr << "Error in server response: unable to retreive email UIDs" << std::endl;
            client.sendCommand("LOGOUT");
            client.disconnect();
            return EXIT_FAILURE;
        }

        // Parse the search response to extract only the numbers (UIDs)
        while (iss >> word)
        {
            // Check if the word is a number (UID)
            if (!word.empty() && std::all_of(word.begin(), word.end(), ::isdigit))
            {
                unseenUIDs.push_back(word);
            }
        }

        if (unseenUIDs.empty())
        {
            std::cout << "No new messages found." << std::endl;
            client.sendCommand("LOGOUT");
            client.disconnect();
            return EXIT_SUCCESS;
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
        std::string uidFetch = "UID SEARCH ALL";
        std::string uidResponse = client.sendCommand(uidFetch);

        fetchCommand = Helpers::GetSynchronizingFetch(args.headers_only, args.mailbox, args.outdir, uidResponse, client.canonical_hostname);

        if (fetchCommand.empty())
        {
            std::cerr << "No new messages to synchronize." << std::endl;
            client.sendCommand("LOGOUT");
            client.disconnect();
            return EXIT_SUCCESS;
        }
    }

    std::string fetchResponse = client.sendCommand(fetchCommand);

    std::vector<std::string> rawEmails;
    std::vector<std::string> UIDs;

    if (!Helpers::ParseImapResponse(fetchResponse, rawEmails, UIDs))
    {
        client.sendCommand("LOGOUT");
        client.disconnect();
        return EXIT_FAILURE;
    };

    // Process and save emails
    int downloadedCount = 0;
    for (size_t i = 0; i < rawEmails.size(); ++i)
    {
        try
        {
            EmailMessage message(rawEmails[i]);
            message.saveToFile(args.outdir, UIDs[i], args.mailbox, client.canonical_hostname, args.headers_only);
            ++downloadedCount;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Error: Failed to process email " << i + 1 << ": " << ex.what() << std::endl;
        }
    }

    if (args.headers_only && args.new_only)
    {
        std::cout << "Downloaded " << downloadedCount << " new messages (headers only) from mailbox " << args.mailbox << std::endl;
    }
    else if (args.headers_only)
    {
        std::cout << "Downloaded " << downloadedCount << " messages (headers only) from mailbox " << args.mailbox << std::endl;
    }
    else if (args.new_only)
    {
        std::cout << "Downloaded " << downloadedCount << " new messages from mailbox " << args.mailbox << std::endl;
    }
    else
    {
        std::cout << "Downloaded " << downloadedCount << " messages from mailbox " << args.mailbox << std::endl;
    }

    client.sendCommand("LOGOUT");
    client.disconnect();

    return EXIT_SUCCESS;
}
