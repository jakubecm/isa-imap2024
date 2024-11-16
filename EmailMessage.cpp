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
    std::string email;  // The email message as a string.

public:
    /**
     * @brief Constructs an EmailMessage object with the given email.
     * @param email The email message as a string.
     */
    EmailMessage(const std::string &email) : email(email) {}

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

        outFile << this->email;

        outFile.close();
    }
};
