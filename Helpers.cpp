/**
 * @file Helpers.cpp
 * @author Milan Jakubec (xjakub41)
 * @date 2024-11-15
 * @brief A file implementing a class for helper functions that are used repeatedly.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

class Helpers
{
public:
    /**
     * @brief Parse the login credentials from the authfile.
     *
     * @param authfile The path to the file containing the login credentials.
     * @return std::string The login credentials in the format "username password".
     */
    static std::string parseLogin(const std::string &authfile)
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
        else
        {
            std::cerr << "Failed to open authfile: " << authfile << std::endl;
            exit(EXIT_FAILURE);
        }

        return username + " " + password;
    }

    /**
     * @brief Get the local UIDs from the output directory.
     *
     * @param outputDir The directory where the emails are saved.
     * @param mailbox The mailbox name.
     * @param canonicalHostname The canonical hostname of the mail server.
     * @param headersOnly UIDs with only headers downloaded.
     * @param fullEmails UIDs with full emails downloaded.
     */
    static void GetLocalUIDs(const std::string &outputDir, const std::string &mailbox, const std::string &canonicalHostname,
                             std::vector<int> &headersOnly, std::vector<int> &fullEmails)
    {
        std::string filePrefix = outputDir + "/" + canonicalHostname + "_" + mailbox + "_";
        for (const auto &entry : fs::directory_iterator(outputDir))
        {
            if (entry.is_regular_file())
            {
                std::string filePath = entry.path().string();
                if (filePath.find(filePrefix) == 0)
                {
                    std::string uidStr = filePath.substr(filePrefix.length(), filePath.length() - filePrefix.length() - 4);

                    // Check if it's a header-only file
                    if (filePath.substr(filePath.length() - 12) == "_headers.eml")
                    {
                        headersOnly.push_back(std::stoi(uidStr));
                    }
                    else if (filePath.substr(filePath.length() - 4) == ".eml")
                    {
                        fullEmails.push_back(std::stoi(uidStr));
                    }
                }
            }
        }
    }

    /**
     * @brief Get the UIDs from the mail server response.
     *
     * @return std::vector<int> The list of UIDs.
     */
    static std::vector<int> GetMailServerUids(const std::string &serverResponse)
    {
        std::vector<int> serverUIDs;
        std::regex uid_regex(R"(\* SEARCH( (\d+))+)");
        std::smatch match;
        std::string::const_iterator searchStart(serverResponse.cbegin());

        if (std::regex_search(searchStart, serverResponse.cend(), match, uid_regex))
        {
            std::string uidList = match[0].str(); // This captures the whole line with UIDs
            std::istringstream iss(uidList);
            std::string uid;
            // Skip the '* SEARCH' part by reading the first two segments which are '* SEARCH'
            iss >> uid; // Read '*'
            iss >> uid; // Read 'SEARCH'

            while (iss >> uid)
            {
                serverUIDs.push_back(std::stoi(uid));
            }
        }

        return serverUIDs;
    }

    /**
     * @brief Get the FETCH command for synchronizing the mailbox based on missing UIDs.
     *
     * @param headersOnly Fetch only the headers.
     * @param mailbox The mailbox name.
     * @param outputDir The directory where the emails are saved.
     * @param uidResponse The response from the UID FETCH command.
     * @param canonicalHostname The canonical hostname of the mail server.
     * @return std::string The FETCH command for missing or upgradeable UIDs.
     */
    static std::string GetSynchronizingFetch(bool headersOnly, const std::string &mailbox, const std::string &outputDir,
                                             const std::string &uidResponse, const std::string &canonicalHostname)
    {
        std::vector<int> headerOnlyUIDs, fullEmailUIDs;
        GetLocalUIDs(outputDir, mailbox, canonicalHostname, headerOnlyUIDs, fullEmailUIDs);
        std::vector<int> serverUIDs = GetMailServerUids(uidResponse);
        std::vector<int> fetchUIDs;

        if (headersOnly)
        {
            // Find UIDs that are missing completely (for headers)
            for (int serverUID : serverUIDs)
            {
                if (std::find(headerOnlyUIDs.begin(), headerOnlyUIDs.end(), serverUID) == headerOnlyUIDs.end() &&
                    std::find(fullEmailUIDs.begin(), fullEmailUIDs.end(), serverUID) == fullEmailUIDs.end())
                {
                    fetchUIDs.push_back(serverUID);
                }
            }
        }
        else
        {
            // Find UIDs that are either missing completely or have only headers (to upgrade)
            for (int serverUID : serverUIDs)
            {
                if (std::find(fullEmailUIDs.begin(), fullEmailUIDs.end(), serverUID) == fullEmailUIDs.end())
                {
                    fetchUIDs.push_back(serverUID);
                }
            }
        }

        if (!fetchUIDs.empty())
        {
            std::string uidList = std::to_string(fetchUIDs[0]);
            for (size_t i = 1; i < fetchUIDs.size(); ++i)
            {
                uidList += "," + std::to_string(fetchUIDs[i]);
            }

            // Generate the FETCH command
            return headersOnly ? "UID FETCH " + uidList + " (UID BODY.PEEK[HEADER])" : "UID FETCH " + uidList + " (UID BODY[])";
        }

        return "";
    }

    /**
     * @brief Ensures the UIDVALIDITY file is generated and valid.
     *
     * @param mailbox The mailbox name.
     * @param outputDir The directory where the UIDVALIDITY file is saved.
     * @param uidvalidity The UIDVALIDITY string from the SELECT command.
     * @param canonicalHostname The canonical hostname of the mail server.
     */
    static void EnsureUIDValidity(const std::string &mailbox, const std::string &outputDir, const std::string &uidvalidity, const std::string &canonicalHostname)
    {
        // Create directory if it doesn't exist
        if (!fs::exists(outputDir))
        {
            fs::create_directories(outputDir);
        }

        // File path for saving the UIDVALIDITY
        std::string file_path = outputDir + "/" + canonicalHostname + "_uidvalidity_" + mailbox;

        // Check if the file exists
        if (fs::exists(file_path))
        {
            // Check if the UIDVALIDITY matches
            std::ifstream uidvalidity_file(file_path);
            if (uidvalidity_file.is_open())
            {
                std::string saved_uidvalidity;
                uidvalidity_file >> saved_uidvalidity;
                uidvalidity_file.close();

                if (saved_uidvalidity == uidvalidity)
                {
                    return; // UIDVALIDITY matches, no further action needed
                }
                else
                {
                    // UIDVALIDITY mismatch: delete local mailbox files
                    for (const auto &entry : fs::directory_iterator(outputDir))
                    {
                        if (entry.is_regular_file())
                        {
                            std::string filePath = entry.path().string();
                            if (filePath.find(outputDir + "/" + mailbox) == 0)
                            {
                                fs::remove(filePath);
                            }
                        }
                    }
                }
            }
            else
            {
                std::cerr << "Failed to open UIDVALIDITY file: " << file_path << std::endl;
                return;
            }
        }

        // Create or overwrite the UIDVALIDITY file
        std::ofstream uidvalidity_file(file_path);
        if (uidvalidity_file.is_open())
        {
            uidvalidity_file << uidvalidity;
            uidvalidity_file.close();
            std::cout << "UIDVALIDITY " << uidvalidity << " saved to " << file_path << std::endl;
            return;
        }
        else
        {
            std::cerr << "Failed to create UIDVALIDITY file: " << file_path << std::endl;
            return;
        }
    }

    /**
     * @brief Handles parsing the select response to find UIDVALIDITY and pass it to handler function EnsureUIDValidity
     *
     * @param mailbox The mailbox name.
     * @param outputDir The directory where the UIDVALIDITY file is saved.
     * @param selectResponse The response from the SELECT command.
     * @param canonicalHostname The canonical hostname of the mail server.
     * @return true if the UIDVALIDITY was found and handled, false otherwise.
     */
    static bool HandleUIDValidity(const std::string &mailbox, const std::string &outputDir, const std::string &selectResponse, const std::string &canonicalHostname)
    {
        // Check for NO or BAD response
        if ((selectResponse.find("NO") != std::string::npos) || (selectResponse.find("BAD") != std::string::npos))
        {
            std::cerr << "Error: Unexpected response from server." << std::endl;
            return false;
        }
        // Regular expression to match the UIDVALIDITY line and capture the number
        std::regex uidvalidity_regex(R"(UIDVALIDITY (\d+))");
        std::smatch match;

        // Search for UIDVALIDITY in the response
        if (std::regex_search(selectResponse, match, uidvalidity_regex) && match.size() > 1)
        {
            std::string uidvalidity = match.str(1);

            EnsureUIDValidity(mailbox, outputDir, uidvalidity, canonicalHostname);
        }
        else
        {
            std::cerr << "Error: UIDVALIDITY not found in server response." << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Handles the response from the LOGIN command to check for authentication failure.
     *
     * @param response The response from the LOGIN command.
     * @return true if the login was successful, false otherwise.
     */
    static bool HandleLoginResponse(const std::string &response)
    {
        // Check if the response contains "NO"
        if (response.find("NO") != std::string::npos)
        {
            std::cerr << "Error: Authentication failed." << std::endl;
            return false;
        }

        // Check if the response contains "BAD"
        if (response.find("BAD") != std::string::npos)
        {
            std::cerr << "Error: Invalid or missing login." << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Parse the IMAP response to extract the raw emails and UIDs.
     *
     * @param fetchResponse The response from the FETCH command.
     * @param rawEmails The vector to store the raw email strings.
     * @param UIDs The vector to store the UIDs.
     * @return true if the parsing was successful, false otherwise.
     */
    static bool ParseImapResponse(const std::string &fetchResponse, std::vector<std::string> &rawEmails, std::vector<std::string> &UIDs)
    {
        std::istringstream stream(fetchResponse);
        std::string line;
        std::string currentEmail;
        bool readingBody = false;
        int expectedBodySize = 0;
        std::string currentUID;

        while (std::getline(stream, line))
        {
            // Check for error tags in the response
            if (!readingBody && ((line.find("NO") != std::string::npos) || (line.find("BAD") != std::string::npos)))
            {
                std::cerr << "Error in server response: " << line << std::endl;
                return false; // Stop parsing on error
            }

            // Skip untagged responses like EXISTS, RECENT, EXPUNGE
            if (line.find("* ") == 0 && (line.find("EXISTS") != std::string::npos || line.find("RECENT") != std::string::npos || line.find("EXPUNGE") != std::string::npos))
            {
                continue;
            }
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

        return true;
    }

    /**
     * @brief Check if headerfile exists and delete it. Used for upgrading from headers to full emails.
     *
     * @param filename The name of the file to check and delete.
     */
    static void CheckIfHeaderFileExistsAndDelete(const std::string &filename)
    {
        std::string headerfile = filename + "_headers.eml";

        if (fs::exists(headerfile))
        {
            fs::remove(headerfile);
        }
    }
};