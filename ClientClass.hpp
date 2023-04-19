#include <iostream>
#include <boost/asio.hpp>

#include "Common.hpp"
#include "json.hpp"

using boost::asio::ip::tcp;

class Client {
public:
	// �������� ��������� �� ������ �� �������.
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

	// ���������� ������ � ������� ������� �� ��������� ������.
	std::string ReadMessage(tcp::socket& aSocket)
	{
		boost::asio::streambuf b;
		boost::asio::read_until(aSocket, b, "\0");
		std::istream is(&b);
		std::string line(std::istreambuf_iterator<char>(is), {});
		return line;
	}

	// "������" ������������, �������� ��� ID.
	std::string ProcessRegistration(tcp::socket& aSocket)
	{
		std::string name;
		std::cout << "Hello! Enter your name: ";
		std::cin >> name;

		// ��� ����������� Id �� �����, �������� ��� ����
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

		// �� ������������, ��� ��� ������������� ������������ ����� �������������� ID.
		// ��� �� "������������" ������������ - ���������� �� ������ ���, � ������ ���������� ��� ID.
		// ���� ID ����� ������������ ��� �������� ��������.
		myId = ProcessRegistration(socket);
	}

	void menu() {
		bool exited = false;
		while (! exited)
		{
			// ��� ����������� "�����������" ����.
			std::cout << "Menu:\n"
				"1) Buy Order\n"
				"2) Sell Order\n"
				"3) Check Balance\n"
				"4) Cancel Order\n"
				"5) Get Active Orders\n"
				"6) Get Deal History\n"
				"7) Get Current Prices\n"
				"0) Exit\n";

			short menu_option_num = 0;
			std::cin >> menu_option_num;
			orderType ot = orderType::sell;
			switch (menu_option_num)
			{
			case 1:
			{
				ot = orderType::buy;
				//no break here, executing code from case 2
			}
			case 2:
			{
				std::string price;
				std::string amount;
				std::cout << "Please enter desired price: ";
				std::cin >> price;
				std::cout << "Please enter amount: ";
				std::cin >> amount;
				nlohmann::json msg;
				msg["Price"] = price;
				msg["Amount"] = amount;
				msg["OrderType"] = ot;
				std::string message = msg.dump();
				SendMessage(socket, myId, Requests::AddOrder, message);
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 3:
			{
				SendMessage(socket, myId, Requests::CheckBalance, "");
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 4:
			{
				std::string orderId;
				std::cout << "Please enter order id to cancel: ";
				std::cin >> orderId;
				SendMessage(socket, myId, Requests::CancelOrder, orderId);
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 5:
			{
				SendMessage(socket, myId, Requests::getActiveOrders, "");
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 6:
			{
				SendMessage(socket, myId, Requests::getDealHistory, "");
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 7:
			{
				SendMessage(socket, myId, Requests::getCurrentPrices, "");
				std::cout << ReadMessage(socket) << std::endl;
				break;
			}
			case 0:
			{
				exited = true;
				break;
			}
			default:
			{
				std::cout << menu_option_num << " is unknown menu option\n" << std::endl;
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
