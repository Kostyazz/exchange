#include <cstdlib>
#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include "json.hpp"
#include "Common.hpp"

using boost::asio::ip::tcp;

class Server
{
private:
    // <UserId, UserName>
    static std::map<size_t, std::string> mUsers;
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;

    // "Регистрирует" нового пользователя и возвращает его ID.
    static std::string RegisterNewUser(const std::string& aUserName)
    {
        size_t newUserId = mUsers.size();
        mUsers[newUserId] = aUserName;

        return std::to_string(newUserId);
    }

    // Запрос имени клиента по ID
    static std::string GetUserName(const std::string& aUserId)
    {
        const auto userIt = mUsers.find(std::stoi(aUserId));
        if (userIt == mUsers.cend())
        {
            return "Error! Unknown User";
        }
        else
        {
            return userIt->second;
        }
    }

    class session
    {
    public:
        session(boost::asio::io_service& io_service)
            : socket_(io_service), data_("")
        {
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        void start()
        {
            socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
                boost::bind(&session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }

        // Обработка полученного сообщения.
        void handle_read(const boost::system::error_code& error,
            size_t bytes_transferred)
        {
            if (!error)
            {
                data_[bytes_transferred] = '\0';

                // Парсим json, который пришёл нам в сообщении.
                auto j = nlohmann::json::parse(data_);
                auto reqType = j["ReqType"];

                std::string reply = "Error! Unknown request type";
                if (reqType == Requests::Registration)
                {
                    // Это реквест на регистрацию пользователя.
                    // Добавляем нового пользователя и возвращаем его ID.
                    reply = RegisterNewUser(j["Message"]);
                }
                else if (reqType == Requests::Hello)
                {
                    // Это реквест на приветствие.
                    // Находим имя пользователя по ID и приветствуем его по имени.
                    reply = "Hello, " + GetUserName(j["UserId"]) + "!\n";
                }

                strcpy(data_, reply.c_str());

                boost::asio::async_write(socket_,
                    boost::asio::buffer(data_, reply.size()),
                    boost::bind(&session::handle_write, this,
                        boost::asio::placeholders::error));
            }
            else
            {
                delete this;
            }
        }

        void handle_write(const boost::system::error_code& error)
        {
            if (!error)
            {
                socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
                    boost::bind(&session::handle_read, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
            }
            else
            {
                delete this;
            }
        }

    private:
        tcp::socket socket_;
        char data_[MAX_LENGTH];
    };

public:
    Server(boost::asio::io_service& io_service)
        : io_service_(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
    {
        std::cout << "Server started! Listen " << port << " port" << std::endl;

        session* new_session = new session(io_service_);
        acceptor_.async_accept(new_session->socket(),
            boost::bind(&Server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
    }

    void handle_accept(session* new_session,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_session->start();
            new_session = new session(io_service_);
            acceptor_.async_accept(new_session->socket(),
                boost::bind(&Server::handle_accept, this, new_session,
                    boost::asio::placeholders::error));
        }
        else
        {
            delete new_session;
        }
    }

};

std::map<size_t, std::string> Server::mUsers;

int main()
{
    try
    {
        boost::asio::io_service io_service;

        Server s(io_service);

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}