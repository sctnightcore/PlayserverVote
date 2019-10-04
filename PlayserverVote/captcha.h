#pragma once

#include <string>
#include <deque>
#include <memory>

#include <curl/curl.h>

#define MAX_CHAR 10000

struct CaptCha {
	std::string CheckSum;
	std::string ImageBase64;
	std::string Text;
	int TaskID = 0;
};

class AntiCaptcha {
public:
	AntiCaptcha(const std::string& InputServiceKey, int InputServerID, const std::string& InputIpAddress, int InputPort, int InputMaxImage, const std::string& GameID);
	~AntiCaptcha();
	void Run();

private:
	int ClearBuffer();
	int CurlInitial();
	CURLcode LoadImageContainer();
	void ChecekTaskService(CaptCha* captchaTask);
	void ReportIncorrectCaptcha(CaptCha* captcha);

	std::string ReadBuffer;
	std::string ServiceKey;
	std::string Base64Image;
	std::string CheckSum;
	std::string CaptchaText;
	std::string IPAddress;
	std::string GameUserID;
	int Port;
	int TaskID;
	int WaitSecond;
	int ServerID;
	int MaxImage;

	CURL* curl;
	CURLcode res;

	std::deque<std::shared_ptr<CaptCha>> CaptchaContainer;
};