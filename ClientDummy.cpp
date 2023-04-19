#include <fstream>
#include <iostream>
#include "Common.hpp"
#include <boost/asio.hpp>
#include "json.hpp"
#include "ClientClass.hpp"

using boost::asio::ip::tcp;

int main() {
	
	freopen("../test.txt", "r", stdin);
	freopen("out.txt", "w", stdout);

	Client dummySeller;
	dummySeller.init();
	dummySeller.menu();

	Client dummyBuyer;
	dummyBuyer.init();
	dummyBuyer.menu();

	return 0;
}