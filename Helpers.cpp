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

        return username + " " + password;
    }

    /**
     * @brief Get the local UIDs from the output directory.
     *
     * @param outputDir The directory where the emails are saved.
     * @param mailbox The mailbox name.
     * @return std::vector<int> The list of local UIDs.
     */
    static std::vector<int> GetLocalUIDs(const std::string &outputDir, const std::string &mailbox)
    {
        std::vector<int> localUIDs;
        std::string filePrefix = outputDir + "/" + mailbox + "_";
        for (const auto &entry : fs::directory_iterator(outputDir))
        {
            if (entry.is_regular_file())
            {
                std::string filePath = entry.path().string();
                if (filePath.find(filePrefix) == 0)
                {
                    std::string uid = filePath.substr(filePrefix.length(), filePath.length() - filePrefix.length() - 4);
                    localUIDs.push_back(std::stoi(uid));
                }
            }
        }

        return localUIDs;
    }

    /**
     * @brief Get the UIDs from the mail server response.
     *
     * @return std::vector<int> The list of UIDs.
     */
    static std::vector<int> GetMailServerUids(const std::string &serverResponse)
    {
        std::vector<int> serverUIDs;
        std::regex uid_regex(R"(\* \d+ FETCH \(UID (\d+)\))");
        std::smatch match;
        std::string::const_iterator searchStart(serverResponse.cbegin());

        while (std::regex_search(searchStart, serverResponse.cend(), match, uid_regex))
        {
            serverUIDs.push_back(std::stoi(match[1].str()));
            searchStart = match.suffix().first;
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
     * @return std::string The FETCH command for missing UIDs.
     */
    static std::string GetSynchronizingFetch(bool headersOnly, const std::string &mailbox, const std::string &outputDir, const std::string &uidResponse)
    {
        // Fetch UIDs
        std::vector<int> localUIDs = GetLocalUIDs(outputDir, mailbox);
        std::vector<int> serverUIDs = GetMailServerUids(uidResponse);
        std::vector<int> missingUIDs;

        // Find missing UIDs that are on the server but not locally
        for (int serverUID : serverUIDs)
        {
            if (std::find(localUIDs.begin(), localUIDs.end(), serverUID) == localUIDs.end())
            {
                missingUIDs.push_back(serverUID);
            }
        }

        if (!missingUIDs.empty())
        {
            std::string uidList = std::to_string(missingUIDs[0]);
            for (size_t i = 1; i < missingUIDs.size(); ++i)
            {
                uidList += "," + std::to_string(missingUIDs[i]);
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
     * @return true if the UIDVALIDITY matches or file is created; false if UIDVALIDITY mismatch.
     */
    static void EnsureUIDValidity(const std::string &mailbox, const std::string &outputDir, const std::string &uidvalidity)
    {
        // Create directory if it doesn't exist
        if (!fs::exists(outputDir))
        {
            fs::create_directories(outputDir);
        }

        // File path for saving the UIDVALIDITY
        std::string file_path = outputDir + "/uidvalidity_" + mailbox;

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
     * @brief Handles parsing the select response, comparing UIDVALIDITY, and managing the UIDVALIDITY file.
     *
     * @param mailbox The mailbox name.
     * @param outputDir The directory where the UIDVALIDITY file is saved.
     * @param selectResponse The response from the SELECT command.
     */
    static void HandleUIDValidity(const std::string &mailbox, const std::string &outputDir, const std::string &selectResponse)
    {
        // Regular expression to match the UIDVALIDITY line and capture the number
        std::regex uidvalidity_regex(R"(UIDVALIDITY (\d+))");
        std::smatch match;

        // Search for UIDVALIDITY in the response
        if (std::regex_search(selectResponse, match, uidvalidity_regex) && match.size() > 1)
        {
            std::string uidvalidity = match.str(1);

            // Use the existing EnsureUIDValidity function to handle the UIDVALIDITY file check
            EnsureUIDValidity(mailbox, outputDir, uidvalidity);
            return;
        }
        else
        {
            std::cerr << "UIDVALIDITY not found in the SELECT response." << std::endl;
            return;
        }
    }
};