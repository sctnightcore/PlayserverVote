#include <iostream>
#include <thread>
#include "captcha.h"
#include "termcolor.hpp"

#pragma comment(lib, "Ws2_32.lib")

void CreateServiceThread(const std::string& ServiceKey, int ServerID, const std::string& IpAddress, int Port, int MaxImage, const std::string& GameID) {
	AntiCaptcha AntiCaptcha(ServiceKey, ServerID, IpAddress, Port, MaxImage, GameID);
	AntiCaptcha.Run();
}

int main()
{
	std::cout << termcolor::cyan << "***********************************************************" << std::endl;
	std::cout << termcolor::cyan << "************* Playserver Post With Proxy  *****************" << std::endl;
	std::cout << termcolor::cyan << "*****  https://github.com/topkung1/PlayserverVote  ********" << std::endl;

	std::thread AntiThread1(CreateServiceThread, "key", 17985, "", 0, 1, "NewGen");

	//AntiThread1.join();

	system("PAUSE");
}