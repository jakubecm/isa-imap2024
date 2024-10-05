#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <filesystem>

class EmailMessage
{
private:
    std::multimap<std::string, std::string> headers;
    std::string body;
    std::string UID;

    // RFC 5322 recommends 78 characters per line and allows up to 998 excluding CRLF
    static constexpr size_t MAX_LINE_LENGTH = 998;

public:
    EmailMessage() {}

    void parseMessage(const std::string &rawEmail)
    {
        std::istringstream emailStream(rawEmail);
        std::string line;
        std::string currentHeaderName;
        std::string currentHeaderValue;
        bool inHeaders = true;

        // Separate headers and body
        while (std::getline(emailStream, line))
        {
            if (line.length() > MAX_LINE_LENGTH)
            {
                throw std::runtime_error("Detected a line too long.");
            }
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

    // Getter for headers and body for debugging
    const std::multimap<std::string, std::string> &getHeaders() const { return headers; }
    const std::string &getBody() const { return body; }

    // Method to print all headers (for debugging)
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

    // Method to save email to a file
    void saveToFile(const std::string &directory, const std::string &messageUid, const std::string &mailboxName)
    {
        std::string fileName = directory + "/" + mailboxName + "_" + messageUid + ".eml";

        // Create directory if it doesn't exist
        std::filesystem::create_directories(directory);

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
        outFile << body;

        outFile.close();
    }
};
