/**
 * @file EmailMessage.cpp
 * @author Milan Jakubec (xjakub41)
 * @date 2024-11-15
 * @brief A file implementing a helper mail message class to parse email messages.
 */

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "Helpers.cpp"

/**
 * @class EmailMessage
 * @brief A class to represent an email message.
 */
class EmailMessage
{
private:
    std::multimap<std::string, std::string> headers; // Multimap to store headers and their values
    std::string body;                                // String to store the email body
    std::string UID;                                 // String to store the email UID

    // RFC 5322 recommends 78 characters per line and allows up to 998 excluding CRLF
    static constexpr size_t MAX_LINE_LENGTH = 1000;

public:
    /**
     * @brief Constructs an EmailMessage object.
     */
    EmailMessage() {}

    /**
     * @brief Parse the raw email message into headers and body.
     * @param rawEmail The raw email message as a string.
     */
    void parseMessage(const std::string &rawEmail)
    {
        std::istringstream emailStream(rawEmail); // Create a stream from the raw email string
        std::string line;                         // String to store the current line
        std::string currentHeaderName;
        std::string currentHeaderValue;
        bool inHeaders = true;

        // Separate headers and body
        while (std::getline(emailStream, line))
        {
            // Remove trailing \r if it exists
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            // End of headers section is marked by an empty line
            if (inHeaders && line.empty())
            {
                inHeaders = false;
                continue;
            }

            if (inHeaders)
            {
                // Handle header continuation (folding)
                if (!currentHeaderName.empty() && (line[0] == ' ' || line[0] == '\t'))
                {
                    // Append continued header value (folding)
                    currentHeaderValue += " " + line.substr(1); // remove leading space/tab
                }
                else
                {
                    // Save the previous header and its value before processing a new header
                    if (!currentHeaderName.empty())
                    {
                        headers.emplace(currentHeaderName, currentHeaderValue);
                    }

                    // Parse new header
                    size_t colonPos = line.find(':');
                    if (colonPos != std::string::npos)
                    {
                        currentHeaderName = line.substr(0, colonPos);
                        currentHeaderValue = line.substr(colonPos + 1);
                        // Trim leading whitespace in the header value
                        currentHeaderValue.erase(0, currentHeaderValue.find_first_not_of(" \t"));
                    }
                    else
                    {
                        // Handle invalid header without a colon (just skip it)
                        currentHeaderName.clear();
                        currentHeaderValue.clear();
                    }
                }
            }
            else
            {
                body += line + "\n";
            }
        }

        // Store the last header
        if (!currentHeaderName.empty())
        {
            headers.emplace(currentHeaderName, currentHeaderValue);
        }
    }

    /**
     * @brief Get the email headers. Serves only as a debugging function.
     */
    void printHeaders() const
    {
        if (headers.empty())
        {
            std::cout << "No headers found!" << std::endl;
        }
        else
        {
            std::cout << "Printing " << headers.size() << " headers:" << std::endl;
            for (const auto &header : headers)
            {
                std::cout << header.first << ": " << header.second << std::endl;
            }
        }
    }

    /**
     * @brief Save the email message to a file.
     * @param directory The directory where the email should be saved.
     * @param messageUid The UID of the email message.
     * @param mailboxName The name of the mailbox the email belongs to.
     * @param canonicalHostname The canonical hostname of the mail server.
     * @param headersOnly Whether to save only the headers.
     */
    void saveToFile(const std::string &directory, const std::string &messageUid, const std::string &mailboxName,
                    const std::string &canonicalHostname, bool headersOnly)
    {

        std::string fileName = directory + "/" + canonicalHostname + "_" + mailboxName + "_" + messageUid;
        Helpers::CheckIfHeaderFileExistsAndDelete(fileName);

        if (headersOnly)
        {
            fileName += "_headers.eml";
        }
        else
        {
            fileName += ".eml";
        }

        std::ofstream outFile(fileName);
        if (!outFile.is_open())
        {
            throw std::runtime_error("Unable to create file: " + fileName);
        }

        // Write headers
        for (const auto &header : headers)
        {
            outFile << header.first << ": " << header.second << "\n";
        }

        // Write a newline to separate headers from body
        outFile << "\n";

        // Write body
        if (!headersOnly)
        {
            outFile << body;
        }

        outFile.close();
    }
};
