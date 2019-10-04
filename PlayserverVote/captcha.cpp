#include "captcha.h"
#include "json.hpp"
#include "base64.h"
#include "termcolor.hpp"
#include <thread>

using json = nlohmann::json;

const char* GetImageJSONPattern = "http://playserver.co/index.php/Vote/ajax_getpic/%d"; // POST checksum, success, wait
const char* GetImageFilePattern = "http://playserver.co/index.php/VoteGetImage/%s"; // GET
const char* PlayserverVoteURLPattern = "http://playserver.co/index.php/Vote/ajax_submitpic/%d"; // POST server_id=17985&captcha=CHNL8F&gameid=tete&checksum=3u2i9qF5Qdx80ffN error_msg, success True, used True, wait 61
const char* PlayserverVotePattern = "server_id=%d&captcha=%s&gameid=%s&checksum=%s";
const char* AntiCaptchaAPI = "{\"clientKey\":\"%s\",\"task\":{\"type\":\"ImageToTextTask\",\"body\":\"%s\",\"phrase\":false,\"case\":false,\"numeric\":0,\"math\":0,\"minLength\":6,\"maxLength\":6}, \"languagePool\":\"en\", \"softId\":921}";
const char* AntiCaptchaTaskPattern = "{\"clientKey\":\"%s\",\"taskId\" : %d}";

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

const std::string vformat(const char* const zcFormat, ...)
{
	// initialize use of the variable argument array
	va_list vaArgs;
	va_start(vaArgs, zcFormat);

	// reliably acquire the size
	// from a copy of the variable argument array
	// and a functionally reliable call to mock the formatting
	va_list vaArgsCopy;
	va_copy(vaArgsCopy, vaArgs);
	const int iLen = std::vsnprintf(NULL, 0, zcFormat, vaArgsCopy);
	va_end(vaArgsCopy);

	// return a formatted string without risking memory mismanagement
	// and without assuming any compiler or platform specific behavior
	std::vector<char> zc(iLen + 1);
	std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
	va_end(vaArgs);
	return std::string(zc.data(), iLen);
}

AntiCaptcha::AntiCaptcha(const std::string& InputServiceKey, int InputServerID, const std::string& InputIpAddress, int InputPort, int InputMaxImage, const std::string& GameID) :
	ServiceKey(InputServiceKey), ServerID(InputServerID), IPAddress(InputIpAddress), Port(InputPort), TaskID(0), WaitSecond(0), MaxImage(InputMaxImage), GameUserID(GameID)
{
	curl = curl_easy_init();
	CurlInitial();
}

AntiCaptcha::~AntiCaptcha()
{
	curl_easy_cleanup(curl);
}

void AntiCaptcha::Run()
{
	while (true) {

		/*
		1. Load Images if it hasn't had one
		*/
		if (LoadImageContainer()) {
			continue;
		}

		/*
		2. Check image if exists
		*/
		if (!CaptchaContainer.size()) {
			continue;
		}

		/*
		2. Select top image to process and create task and post
		*/

		std::shared_ptr<CaptCha> CaptchaPtr = CaptchaContainer.front();

		ChecekTaskService(CaptchaPtr.get());

		// In case, text hasn't yet solved
		if (!CaptchaPtr->Text.length()) {
			std::this_thread::sleep_for(std::chrono::seconds(5));
			continue;
		}

		// In case, captcha is solved pop out first
		CaptchaContainer.pop_front();

		/*
		3. post to playserver
		*/
		ClearBuffer();

		// Create Json Data
		char PlayserverVoteURLFormatted[1000];
		sprintf(&PlayserverVoteURLFormatted[0], PlayserverVoteURLPattern, 17985);
		
		char PlayerserverVotedDataFormmated[1000];
		sprintf(&PlayerserverVotedDataFormmated[0], PlayserverVotePattern, ServerID, CaptchaPtr->Text.c_str(), GameUserID.c_str(), CaptchaPtr->CheckSum.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, PlayserverVoteURLFormatted);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PlayerserverVotedDataFormmated);

		res = curl_easy_perform(curl);

		if (res) {
			std::cout << termcolor::red << vformat("%s:%d [CURL Error] : %d | %s\n", IPAddress.c_str(), Port, res, PlayserverVoteURLFormatted);
			continue; // Problem
		}

		if (!ReadBuffer.length())
			continue;

		if (ReadBuffer[0] != '{')
			continue;

		json j = nlohmann::json::parse(ReadBuffer);

		if (j["success"].get<bool>() == false) {
			std::cout << termcolor::red << vformat("[Thread ID 0x%p] %s:%d vote failed [%s : %s]\n", &std::this_thread::get_id(), IPAddress.c_str(), Port, CaptchaPtr->CheckSum.c_str(), CaptchaPtr->Text.c_str());
			ReportIncorrectCaptcha(CaptchaPtr.get());
			continue;
		}

		std::cout << termcolor::green << vformat("[Thread ID 0x%p] %s:%d vote succecced [%s : %s]\n", &std::this_thread::get_id(), IPAddress.c_str(), Port, CaptchaPtr->CheckSum.c_str(), CaptchaPtr->Text.c_str());

		WaitSecond = j["wait"].get<UINT32>();

		std::this_thread::sleep_for(std::chrono::seconds(WaitSecond));
	}
}

int AntiCaptcha::ClearBuffer()
{
	ReadBuffer.clear();
	return 0;
}

int AntiCaptcha::CurlInitial()
{
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ReadBuffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/77.0.3865.90 Safari/537.36");

	if (IPAddress.length()) {
		char ProxyFormatted[MAX_CHAR];
		sprintf(&ProxyFormatted[0], "http://%s:%d", IPAddress.c_str(), Port);
		curl_easy_setopt(curl, CURLOPT_PROXY, ProxyFormatted);
	}
	else {
		IPAddress = "localhost";
	}

	return 0;
}

CURLcode AntiCaptcha::LoadImageContainer()
{
	// if there are any images left then continue
	if (CaptchaContainer.size()) {
		return CURLE_OK;
	}

	char GetImageURLFormatted[MAX_CHAR];
	sprintf(&GetImageURLFormatted[0], GetImageJSONPattern, ServerID);

	json j;

	for (int Count = 0; Count < MaxImage; ++Count) 
	{
		/*******************************
		Get Image JSON from Playerserver
		*******************************/
		ClearBuffer();
		curl_easy_setopt(curl, CURLOPT_URL, GetImageURLFormatted);
		res = curl_easy_perform(curl);

		if (res) {
			std::cout << termcolor::red << vformat("%s:%d [CURL Error] : %d | %s\n", IPAddress.c_str(), Port, res, GetImageURLFormatted);
			return res;
		}

		if (!ReadBuffer.length())
			continue;

		if (ReadBuffer[0] != '{')
			continue;

		j = nlohmann::json::parse(ReadBuffer);

		if (j["success"].get<bool>() != true) {
			continue;
		}

		std::shared_ptr<CaptCha> CaptchaPtr = std::make_shared<CaptCha>();
		CaptchaPtr->CheckSum = j["checksum"].get<std::string>();

		/*******************************
		Load Image and convert to base64
		*******************************/
		ClearBuffer();

		// Create char contains url
		char GetImageDataFormatted[MAX_CHAR];
		sprintf(&GetImageDataFormatted[0], GetImageFilePattern, CaptchaPtr->CheckSum.c_str());

		// Get Image
		curl_easy_setopt(curl, CURLOPT_URL, GetImageDataFormatted);

		res = curl_easy_perform(curl);

		if (res) {
			std::cout << termcolor::red << vformat("%s:%d [CURL Error] : %d | %s\n", IPAddress.c_str(), Port, res, GetImageDataFormatted);
			return res;
		}

		CaptchaPtr->ImageBase64 = base64_encode(reinterpret_cast<const unsigned char*>(ReadBuffer.c_str()), ReadBuffer.length());
		/*
		END Convert To base64
		*/

		/********************************************
		Create task image and post to decode service
		*********************************************/
		ClearBuffer();

		// Create Json Data
		char APIDecodeServiceURL[10000];
		sprintf(&APIDecodeServiceURL[0], AntiCaptchaAPI, ServiceKey.c_str(), CaptchaPtr->ImageBase64.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, "http://api.anti-captcha.com/createTask");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, APIDecodeServiceURL);

		res = curl_easy_perform(curl);

		if (res) {
			std::cout << termcolor::red << vformat("%s:%d [CURL Error] : %d | http://api.anti-captcha.com/createTask\n", IPAddress.c_str(), Port, res);
			return res;
		}

		if (!ReadBuffer.length())
			continue;

		if (ReadBuffer[0] != '{')
			continue;

		j = nlohmann::json::parse(ReadBuffer);

		if (j["errorId"].get<UINT32>() != 0) {
			std::cout << termcolor::red << vformat("[Thread ID 0x%p] %s:%d has quit [%s] [%s]\n", &std::this_thread::get_id(), IPAddress.c_str(), Port, j["errorCode"].get<std::string>().c_str(), j["errorDescription"].get<std::string>().c_str());
			continue;
		}

		CaptchaPtr->TaskID = j["taskId"].get<UINT32>();

		CaptchaContainer.push_back(CaptchaPtr);
	}

	return CURLE_OK;
}

void AntiCaptcha::ChecekTaskService(CaptCha* captchaTask)
{
	if (!captchaTask)
		return;

	// Clear Old Data
	ClearBuffer();

	// Create Json Data
	char ServiceTaskFormatted[MAX_CHAR];
	sprintf(&ServiceTaskFormatted[0], AntiCaptchaTaskPattern, ServiceKey.c_str(), captchaTask->TaskID);

	curl_easy_setopt(curl, CURLOPT_URL, "http://api.anti-captcha.com/getTaskResult");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ServiceTaskFormatted);

	res = curl_easy_perform(curl);

	if (res) {
		std::cout << termcolor::red << vformat("%s:%d [CURL Error] : %d | http://api.anti-captcha.com/getTaskResult\n", IPAddress.c_str(), Port, res);
		return;
	}

	if (!ReadBuffer.length()) {
		return;
	}

	if (ReadBuffer[0] != '{') {
		return;
	}

	json j = nlohmann::json::parse(ReadBuffer);

	if (j["errorId"].get<UINT32>() == 0 && j["status"].get<std::string>() == "ready") {
		captchaTask->Text = j["solution"]["text"].get<std::string>();
	}
}

void AntiCaptcha::ReportIncorrectCaptcha(CaptCha* captcha)
{
	ClearBuffer();

	// Create Json Data
	char ServiceTaskFormatted[MAX_CHAR];
	sprintf(&ServiceTaskFormatted[0], AntiCaptchaTaskPattern, ServiceKey.c_str(), captcha->TaskID);

	curl_easy_setopt(curl, CURLOPT_URL, "http://api.anti-captcha.com/reportIncorrectImageCaptcha");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ServiceTaskFormatted);
	curl_easy_perform(curl);
}

