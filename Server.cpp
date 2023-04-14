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
        Order() = delete;
        Order(Order*) = delete;
    };

    struct User {
    public:
        static std::atomic<size_t> userId;
        size_t id_;
        std::string name_;
        double usdBalance = 0;
        double rubBalance = 0;
        User(std::string name) :
            name_(name),
            id_(++userId)
        {}
        User() = delete;
        User(User*) = delete;
    };

    // <UserId, UserName>
    std::map<size_t, std::shared_ptr<User> > mUsers;
    std::shared_mutex mUsersMutex;
    // <price, *order>
    std::multimap<double, std::shared_ptr<Order>, std::greater<double> > mBuyOrders;
    std::multimap<double, std::shared_ptr<Order> > mSellOrders;
    std::multimap<size_t, std::shared_ptr<Order> > mUserOrders;
    std::shared_mutex mOrdersMutex;
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;

    // "Регистрирует" нового пользователя и возвращает его ID.
    std::string RegisterNewUser(const std::string& aUserName)
    {
        mUsersMutex.lock();
        std::shared_ptr<User> newUser(std::make_shared<User>(aUserName));
        size_t newUserId = newUser->id_;
        mUsers[newUserId] = newUser;

        mUsersMutex.unlock();
        return std::to_string(newUserId);
    }

    //обработка сделок с новой заявкой
    void processNewOrder(std::shared_ptr<Order> newOrder) {
        //mutex locks before and after this function call
        

        //check buy orders multimap by default
        auto it = mBuyOrders.begin();
        auto itEnd = mBuyOrders.end();
        bool typeIsBuy = newOrder->type_ == orderType::buy;
        if (typeIsBuy) {
            //if order is for buy, check sell orders multimap
            it = mSellOrders.begin();
            itEnd = mSellOrders.end();
        }
        for ( ; it != itEnd && 
            (typeIsBuy ? it->second->price_ <= newOrder->price_
                       : it->second->price_ >= newOrder->price_) ; ) {
            std::shared_ptr<Order> oldOrder = it->second;
            double price = oldOrder->price_;
            double amount = std::min(oldOrder->amount_, newOrder->amount_);
            oldOrder->amount_ -= amount;
            newOrder->amount_ -= amount;
            if (typeIsBuy) {
                mUsers[oldOrder->userId_]->usdBalance -= amount;
                mUsers[oldOrder->userId_]->rubBalance += price * amount;
                mUsers[newOrder->userId_]->usdBalance += amount;
                mUsers[newOrder->userId_]->rubBalance -= price * amount;                
            } else {
                mUsers[oldOrder->userId_]->usdBalance += amount;
                mUsers[oldOrder->userId_]->rubBalance -= price * amount;
                mUsers[newOrder->userId_]->usdBalance -= amount;
                mUsers[newOrder->userId_]->rubBalance += price * amount;
            }
            //TODO notify users
            if (oldOrder->amount_ <= 0) {
                if (typeIsBuy)
                    it = mSellOrders.erase(it);
                else
                    it = mBuyOrders.erase(it);
            } else
                it++;
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
        return "Your order has been processed succesfully";
    }

    std::string getBalance(size_t userId) {
        std::shared_ptr<User> user = mUsers[userId];
        mUsersMutex.lock_shared();

        std::string retValue = "Your balance is: " + std::to_string(user->rubBalance) + " RUB, " + std::to_string(user->usdBalance) + " USD.";

        mUsersMutex.unlock_shared();
        return retValue;
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
        Session() = delete;
        Session(Session*) = delete;

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
                std::string reqType = std::string(j["ReqType"]);

                std::string reply = "Error! Unknown request type";
                if (reqType == Requests::Registration)
                {
                    // Это реквест на регистрацию пользователя.
                    // Добавляем нового пользователя и возвращаем его ID.
                    reply = parent.RegisterNewUser(j["Message"]);
                }
                else if (reqType == Requests::AddOrder)
                {
                    //
                    auto msg = nlohmann::json::parse(std::string(j["Message"]));
                    reply = parent.addOrder(
                        std::stoi(std::string(j["UserId"])),
                        std::stod(std::string(msg["Price"])),
                        std::stod(std::string(msg["Amount"])),
                        orderType(msg["OrderType"]) );
                }
                else if (reqType == Requests::CheckBalance)
                {
                    //
                    reply = parent.getBalance(std::stoi(std::string(j["UserId"])));
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