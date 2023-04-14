#include <cstdlib>
#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include "json.hpp"
#include "Common.hpp"
#include <shared_mutex>
#include <map>


using boost::asio::ip::tcp;

class Server
{
private:    
    struct Order {
    public:
        static std::atomic<size_t> orderId;
        size_t id_;
        size_t userId_;
        double price_;
        double amount_;
        orderType type_;
        Order(size_t userId, double price, double amount, orderType type)
            :userId_(userId),
            price_(price),
            amount_(amount),
            type_(type),
            id_(++orderId)
        {}
    };

    // <UserId, UserName>
    std::map<size_t, std::string> mUsers;
    std::shared_mutex mUsersMutex;
    // <price, *order>
    std::multimap<double, std::shared_ptr<Order> > mBuyOrders;
    std::multimap<double, std::shared_ptr<Order> > mSellOrders;
    std::multimap<size_t, std::shared_ptr<Order> > mUserOrders;
    std::shared_mutex mOrdersMutex;
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;

    // "Регистрирует" нового пользователя и возвращает его ID.
    std::string RegisterNewUser(const std::string& aUserName)
    {
        mUsersMutex.lock();

        size_t newUserId = mUsers.size();
        mUsers[newUserId] = aUserName;

        mUsersMutex.unlock();
        return std::to_string(newUserId);
    }

    // Запрос имени клиента по ID
    std::string GetUserName(size_t userId)
    {
        mUsersMutex.lock_shared();

        const auto userIt = mUsers.find(userId);
        if (userIt == mUsers.cend())
        {
            return "Error! Unknown User";
        }
        else
        {
            return userIt->second;
        }

        mUsersMutex.unlock_shared();
    }

    //обработка сделок с новой заявкой
    void processNewOrder(std::shared_ptr<Order> newOrder) {
        //mutex locks before and after this function call
        
        //if new order sells, check buy orders multimap
        if (newOrder->type_ == orderType::sell) {
            auto it = mBuyOrders.begin();
            auto itEnd = mBuyOrders.end();
            for ( ; it != itEnd && it->second->price_ >= newOrder->price_; ) {
                auto oldOrder = it->second;
                double price = oldOrder->price_;
                double amount = std::min(oldOrder->amount_, newOrder->amount_);
                oldOrder->amount_ -= amount;
                newOrder->amount_ -= amount;
                //todo change balances and notify users
                if (oldOrder->amount_ <= 0) {
                    it = mBuyOrders.erase(it);
                } else
                    it++;
            }
        //check sell orders multimap
        } else {
            auto it = mSellOrders.begin();
            auto itEnd = mSellOrders.end();
            for (; it != itEnd && it->second->price_ <= newOrder->price_; ) {
                auto oldOrder = it->second;
                double price = oldOrder->price_;
                double amount = std::min(oldOrder->amount_, newOrder->amount_);
                oldOrder->amount_ -= amount;
                newOrder->amount_ -= amount;
                //todo change balances and notify users
                if (oldOrder->amount_ <= 0) {
                    it = mSellOrders.erase(it);
                } else
                    it++;
            }
        }
    }

    //Регистрация новой заявки
    std::string addOrder(size_t userId, double price, double amount, orderType type) {
        std::shared_ptr<Order> newOrder = std::make_shared<Order>(userId, price, amount, type);
        mOrdersMutex.lock();

        mUserOrders.insert({ userId, newOrder });
        processNewOrder(newOrder);
        if (newOrder->amount_ > 0) {
            if (type == sell) {
                mSellOrders.insert({ price, newOrder });
            }
            else {
                mBuyOrders.insert({ price, newOrder });
            }
        }

        mOrdersMutex.unlock();
        return "Your order was processed succesfully";
    }

    class Session
    {
    public:
        Session(Server& outer, boost::asio::io_service& io_service)
            : socket_(io_service),
            data_(""),
            parent(outer)
        {
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        void start()
        {
            socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
                boost::bind(&Session::handle_read, this,
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
                    reply = parent.RegisterNewUser(j["Message"]);
                }
                else if (reqType == Requests::Hello)
                {
                    // Это реквест на приветствие.
                    // Находим имя пользователя по ID и приветствуем его по имени.
                    reply = "Hello, " + parent.GetUserName(std::stoi(std::string(j["UserId"]))) + "!\n";
                }
                else if (reqType == Requests::AddOrder)
                {
                    //
                    auto msg = nlohmann::json::parse(std::string(j["Message"]));
                    std::cerr << j << std::endl << msg["OrderType"] << std::endl;
                    reply = parent.addOrder(
                        std::stoi(std::string(j["UserId"])),
                        std::stod(std::string(msg["Price"])),
                        std::stod(std::string(msg["Amount"])),
                        orderType(msg["OrderType"]) );
                }
                strcpy(data_, reply.c_str());

                boost::asio::async_write(socket_,
                    boost::asio::buffer(data_, reply.size()),
                    boost::bind(&Session::handle_write, this,
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
                    boost::bind(&Session::handle_read, this,
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
        Server& parent;
    };

public:
    Server(boost::asio::io_service& io_service)
        : io_service_(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
    {
        std::cout << "Server started! Listen " << port << " port" << std::endl;

        Session* new_session = new Session(*this, io_service_);
        acceptor_.async_accept(new_session->socket(),
            boost::bind(&Server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
    }

    void handle_accept(Session* new_session,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_session->start();
            new_session = new Session(*this, io_service_);
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

std::atomic<size_t> Server::Order::orderId = 0;
std::atomic<size_t> Server::User::userId = 0;

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