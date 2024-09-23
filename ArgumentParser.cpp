#include "ArgumentParser.h"
#include <getopt.h>
#include <iostream>
#include <cstdlib>

ArgumentParser::ArgumentParser(int argc, char *argv[]) : argc(argc), argv(argv) {}

// Funkce pro vypsání nápovědy
void ArgumentParser::print_usage()
{
    std::cerr << "Usage: " << argv[0] << " server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir\n";
}

// Funkce pro parsování argumentů
ArgumentParser::ParsedArgs ArgumentParser::parse()
{
    ParsedArgs args;
    int opt;

    // Definice dlouhých možností
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

    // Zpracování argumentů pomocí getopt_long
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

    // Poslední argument by měl být server (adresovaný přímo na argv[optind])
    if (optind < argc)
    {
        args.server = argv[optind];
    }
    else
    {
        std::cerr << "Error: Server not specified.\n";
        print_usage();
        exit(1);
    }

    // Nastavení výchozího portu podle toho, zda je TLS aktivní
    if (args.port == 0)
    {
        args.port = args.use_tls ? 993 : 143;
    }

    // Ověření povinných parametrů
    if (args.authfile.empty() || args.outdir.empty())
    {
        std::cerr << "Error: Parametrers -a (auth_file) a -o (out_dir) are required!\n";
        print_usage();
        exit(1);
    }

    return args;
}
