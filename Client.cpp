#include "ClientClass.hpp"

int main()
{
    try
    {
        Client client;
        client.init();
        client.menu();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}