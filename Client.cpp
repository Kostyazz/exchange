#include <iostream>
#include <boost/asio.hpp>

#include "Common.hpp"
#include "json.hpp"

using boost::asio::ip::tcp;

class Client {
public:
    // Отправка сообщения на сервер по шаблону.
    void SendMessage(
        tcp::socket& aSocket,
        const std::string& aId,
        const std::string& aRequestType,
        const std::string& aMessage)
    {
        nlohmann::json req;
        req["UserId"] = aId;
        req["ReqType"] = aRequestType;
        req["Message"] = aMessage;

        std::string request = req.dump();
        boost::asio::write(aSocket, boost::asio::buffer(request, request.size()));
    }

    // Возвращает строку с ответом сервера на последний запрос.
    std::string ReadMessage(tcp::socket& aSocket)
    {
        boost::asio::streambuf b;
        boost::asio::read_until(aSocket, b, "\0");
        std::istream is(&b);
        std::string line(std::istreambuf_iterator<char>(is), {});
        return line;
    }

    // "Создаём" пользователя, получаем его ID.
    std::string ProcessRegistration(tcp::socket& aSocket)
    {
        std::string name;
        std::cout << "Hello! Enter your name: ";
        std::cin >> name;

        // Для регистрации Id не нужен, заполним его нулём
        SendMessage(aSocket, "0", Requests::Registration, name);
        return ReadMessage(aSocket);
    }

    Client()
        : socket(io_service), 
        resolver(io_service), 
        iterator(resolver.resolve(query)), 
        query(tcp::v4(), serverIp, std::to_string(port)), 
        io_service()
    {}

    void init() {
        socket.connect(*iterator);

        // Мы предполагаем, что для идентификации пользователя будет использоваться ID.
        // Тут мы "регистрируем" пользователя - отправляем на сервер имя, а сервер возвращает нам ID.
        // Этот ID далее используется при отправке запросов.
        myId = ProcessRegistration(socket);
    }

    void menu() {
        while (true)
        {
            // Тут реализовано "бесконечное" меню.
            std::cout << "Menu:\n"
                "1) Hello Request\n"
                "2) Exit\n"
                << std::endl;

            short menu_option_num;
            std::cin >> menu_option_num;
            switch (menu_option_num)
            {
            case 1:
            {
                // Для примера того, как может выглядить взаимодействие с сервером
                // реализован один единственный метод - Hello.
                // Этот метод получает от сервера приветствие с именем клиента,
                // отправляя серверу id, полученный при регистрации.
                SendMessage(socket, myId, Requests::Hello, "");
                std::cout << ReadMessage(socket);
                break;
            }
            case 2:
            {
                exit(0);
                break;
            }
            default:
            {
                std::cout << "Unknown menu option\n" << std::endl;
            }
            }
        }
    }
private:
    boost::asio::io_service io_service;
    tcp::resolver resolver;
    tcp::resolver::query query;
    tcp::resolver::iterator iterator;
    tcp::socket socket;
    std::string myId = "";
};

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