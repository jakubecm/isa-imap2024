#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>

class EmailMessage
{
private:
    std::multimap<std::string, std::string> headers;
    std::string body;
    bool inBody = false;

    // RFC 5322 recommends 78 characters per line and allows up to 998 excluding CRLF
    static constexpr size_t MAX_LINE_LENGTH = 998;

public:
    EmailMessage() {}

    /**
     * @brief Parse a string containing an email message into headers and body
     *
     * @param message The string containing the email message
     */
    void parse(const std::string &rawMessage)
    {
        std::istringstream unparsedMessage(rawMessage);
        std::string line;
        std::string currentHeaderField;
        std::string currentHeaderValue;

        while (std::getline(unparsedMessage, line))
        {
            if (line.length() > MAX_LINE_LENGTH)
            {
                throw std::runtime_error("Line exceeds 998 characters, which is violating RFC 5322");
            }

            if (line == "\r" || line.empty())
            {
                // Empty line, body starts
                inBody = true;
                continue;
            }

            if (inBody)
            {
                body += line + "\n";
            }
            else
            {
                // Handling headers
                if (line[0] == ' ' || line[0] == '\t')
                {
                    // Continuation of the previous header (folded header)
                    if (currentHeaderField.empty())
                    {
                        throw std::runtime_error("Found a continuation line without a preceding header.");
                    }
                    currentHeaderValue += " " + line; // Append the continuation to the header value
                }
                else
                {
                    // A new header field is encountered
                    if (!currentHeaderField.empty())
                    {
                        // Store the previous header before starting a new one
                        headers.insert({currentHeaderField, currentHeaderValue});
                    }

                    size_t colonPos = line.find(':');
                    if (colonPos == std::string::npos)
                    {
                        throw std::runtime_error("Invalid header format: missing ':' separator.");
                    }

                    currentHeaderField = line.substr(0, colonPos);
                    currentHeaderValue = line.substr(colonPos + 1);

                    // Trim leading spaces in the header value
                    size_t start = currentHeaderValue.find_first_not_of(" \t");
                    if (start != std::string::npos)
                    {
                        currentHeaderValue = currentHeaderValue.substr(start);
                    }
                    else
                    {
                        currentHeaderValue.clear(); // Empty value if there's nothing after the colon
                    }
                }
            }
        }

        if (!currentHeaderField.empty())
        {
            headers.insert({currentHeaderField, currentHeaderValue});
        }
    }

    // Method to get all headers (for debugging or logging)
    void printHeaders() const
    {
        for (const auto &header : headers)
        {
            std::cout << header.first << ": " << header.second << std::endl;
        }
    }

    // Method to get the body of the email
    std::string getBody() const
    {
        return body;
    }
};