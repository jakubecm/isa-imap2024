/**
 * @file ArgumentParser.cpp
 * @author Milan Jakubec (xjakub41)
 * @date 2024-11-15
 * @brief Implementation of the ArgumentParser class for parsing command line arguments.
 */

#include "ArgumentParser.h"
#include <getopt.h>
#include <iostream>
#include <cstdlib>

/**
 * @brief Constructs an ArgumentParser object with provided command-line arguments.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 */
ArgumentParser::ArgumentParser(int argc, char *argv[]) : argc(argc), argv(argv) {}

/**
 * @brief Prints usage information for the program.
 *
 * This function writes the usage information of the program to err output.
 */
void ArgumentParser::print_usage()
{
    std::cerr << "Usage: " << argv[0] << " server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir\n";
}

/**
 * @brief Parses the command-line arguments and returns a ParsedArgs structure.
 *
 * @return ParsedArgs A structure containing all parsed arguments.
 *
 * This function processes command-line options and arguments using getopt_long.
 * It validates required parameters and sets default values where applicable.
 *
 * @exception Exits the program with an error message if mandatory parameters are missing
 *            or invalid arguments are provided.
 */
ArgumentParser::ParsedArgs ArgumentParser::parse()
{
    ParsedArgs args;
    int opt;

    /**
     * Define long options for the program.
     */
    struct option long_options[] = {
        {"port", required_argument, nullptr, 'p'},
        {"tls", no_argument, nullptr, 'T'},
        {"certfile", required_argument, nullptr, 'c'},
        {"certaddr", required_argument, nullptr, 'C'},
        {"new", no_argument, nullptr, 'n'},
        {"headers", no_argument, nullptr, 'h'},
        {"authfile", required_argument, nullptr, 'a'},
        {"mailbox", required_argument, nullptr, 'b'},
        {"outdir", required_argument, nullptr, 'o'},
        {nullptr, 0, nullptr, 0}};

    // Process command-line options using getopt_long
    while ((opt = getopt_long(argc, argv, "p:Tc:C:nha:b:o:", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
        case 'p':
            args.port = std::stoi(optarg);
            break;
        case 'T':
            args.use_tls = true;
            break;
        case 'c':
            if (args.use_tls)
            {
                args.certfile = optarg;
            }
            else
            {
                std::cerr << "Error: Parametr -c (certfile) is only used with -T (TLS).\n";
                print_usage();
                exit(1);
            }
            break;
        case 'C':
            if (args.use_tls)
            {
                args.certaddr = optarg;
            }
            else
            {
                std::cerr << "Error: Parametr -C (certaddr) is only used with -T (TLS).\n";
                print_usage();
                exit(1);
            }
            break;
        case 'n':
            args.new_only = true;
            break;
        case 'h':
            args.headers_only = true;
            break;
        case 'a':
            args.authfile = optarg;
            break;
        case 'b':
            args.mailbox = optarg;
            break;
        case 'o':
            args.outdir = optarg;
            break;
        default:
            print_usage();
            exit(1);
        }
    }

    // The last argument should be the server address
    if ((argc - optind) == 1) // Exactly one positional argument is expected
    {
        args.server = argv[optind];
    }
    else
    {
        std::cerr << "Error: Expected exactly one server argument. Found "
                  << (argc - optind) << " arguments.\n";
        print_usage();
        exit(1);
    }

    // Port setup based on whether TLS is on/off
    if (args.port == 0)
    {
        args.port = args.use_tls ? 993 : 143;
    }

    // Mandatory parameters validation
    if (args.authfile.empty() || args.outdir.empty())
    {
        std::cerr << "Error: Parametrers -a (auth_file) a -o (out_dir) are required!\n";
        print_usage();
        exit(1);
    }

    return args;
}
