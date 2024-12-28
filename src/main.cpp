
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <stdexcept>
#include <iomanip>
#include <opencv2/opencv.hpp>

const double FIND_TEMPLATE_THRESHOLD = 0.85;
const char *WINDOW_NAME = "Lineage II";
constexpr char *PORT_NAME = "COM3";

struct Images
{
    cv::Mat lowHpImage;
    cv::Mat cursorOnTargetImage;
    cv::Mat targetSelectedImage;
    cv::Mat targetIsDeadImage;
    cv::Mat targetIsAliveImage;

    Images()
    {
        lowHpImage = cv::imread("C:/Users/pc/Downloads/L2IL_Lionna/system/low_hp.jpg");
        cursorOnTargetImage = cv::imread("C:/Users/pc/Downloads/L2IL_Lionna/system/cursor_on_target.jpg");
        targetSelectedImage = cv::imread("C:/Users/pc/Downloads/L2IL_Lionna/system/target_selected.jpg");
        targetIsDeadImage = cv::imread("C:/Users/pc/Downloads/L2IL_Lionna/system/target_is_dead.jpg");
        targetIsAliveImage = cv::imread("C:/Users/pc/Downloads/L2IL_Lionna/system/target_is_alive.jpg");
    }
};

class SerialPort
{
private:
    HANDLE hSerial;

public:
    explicit SerialPort()
    {
        hSerial = CreateFile(PORT_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hSerial == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to open port " + std::string(PORT_NAME));
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(hSerial, &dcbSerialParams))
        {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to get port state");
        }

        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        if (!SetCommState(hSerial, &dcbSerialParams))
        {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to set port state");
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.WriteTotalTimeoutConstant = 50;
        if (!SetCommTimeouts(hSerial, &timeouts))
        {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to set port timeouts");
        }
    }

    ~SerialPort()
    {
        if (hSerial != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hSerial);
        }
    }

    void write(const std::string &message)
    {

        DWORD bytesWritten;
        if (!WriteFile(hSerial, message.c_str(), message.size(), &bytesWritten, NULL))
        {
            throw std::runtime_error("Error writing to port");
        }
    }
};

void moveCursor(int newX, int newY, SerialPort &s)
{

    POINT current;
    int deltaX = 0;
    int deltaY = 0;

    GetCursorPos(&current);

    deltaX = newX - current.x;
    deltaY = newY - current.y;

    std::string message = "move_" + std::to_string(deltaX) + "_" + std::to_string(deltaY) + "\n";
    s.write(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void mouseClick(SerialPort &s)
{
    s.write("mouse_left_press\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    s.write("mouse_left_release\n");
}
void attack(SerialPort &s)
{
    s.write("1\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
void escape(SerialPort &s)
{
    s.write("esc\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

RECT getWindowRect()
{
    HWND hwnd = FindWindow(nullptr, WINDOW_NAME);
    if (!hwnd)
    {
        std::wcerr << L"Окно \"" << WINDOW_NAME << L"\" не найдено!" << std::endl;
        return RECT{0, 0, 0, 0};
    }

    RECT rect;
    if (GetWindowRect(hwnd, &rect))
    {
        return rect;
    }
    else
    {
        std::wcerr << L"Не удалось получить границы окна!" << std::endl;
        return RECT{0, 0, 0, 0};
    }
}

cv::Mat capture()
{
    RECT rect = getWindowRect();
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hScreenDC = GetDC(nullptr);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, rect.left, rect.top, SRCCOPY);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Flip vertically
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    cv::Mat img(height, width, CV_8UC3);
    GetDIBits(hMemoryDC, hBitmap, 0, height, img.data, reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);

    return img;
}

bool findTemplate(const cv::Mat &screen, const cv::Mat &source, double threshold = FIND_TEMPLATE_THRESHOLD)
{
    cv::Mat result;
    cv::Point matchLoc;
    cv::matchTemplate(screen, source, result, cv::TM_CCOEFF_NORMED);
    double minVal, maxVal;
    cv::minMaxLoc(result, &minVal, &maxVal, nullptr, &matchLoc);
    return maxVal >= threshold;
}

double euclideanDistance(const cv::Point &p1, const cv::Point &p2)
{
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

std::vector<cv::Point> groupedPoints(const std::vector<cv::Point> &points, double maxDistance)
{

    std::vector<std::vector<cv::Point>> groups;
    std::vector<bool> visited(points.size(), false);

    for (size_t i = 0; i < points.size(); ++i)
    {
        if (visited[i])
            continue;

        std::vector<cv::Point> group;
        group.push_back(points[i]);
        visited[i] = true;

        for (size_t j = 0; j < points.size(); ++j)
        {
            if (!visited[j] && euclideanDistance(points[i], points[j]) <= maxDistance)
            {
                group.push_back(points[j]);
                visited[j] = true;
            }
        }

        groups.push_back(group);
    };

    std::vector<cv::Point> centers;

    for (const auto &group : groups)
    {
        float sumX = 0, sumY = 0;
        for (const auto &point : group)
        {
            sumX += point.x;
            sumY += point.y;
        }

        cv::Point center(static_cast<int>(sumX / group.size()), static_cast<int>(sumY / group.size()));
        centers.push_back(center);
    }

    return centers;
}

std::vector<cv::Point> getTargets(bool debug, cv::Mat &windowImage, RECT &r, int maxTargets = 5)
{
    cv::Mat image = capture();
    cv::Mat gray;
    cv::Mat blurred;
    cv::Mat edges;
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    std::vector<cv::Point> monsterCenters;
    std::vector<cv::Point> out;

    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edges, 40, 100);
    cv::findContours(edges, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::Point2f screenCenter(image.cols / 2.0f, image.rows / 2.0f);

    windowImage = image.clone();

    cv::circle(windowImage, screenCenter, 5, cv::Scalar(0, 255, 0), -1);

    for (const auto &contour : contours)
    {

        double area = cv::contourArea(contour);
        if (area < 10 || area > 1000)
        {
            continue;
        }

        cv::Moments m = cv::moments(contour);
        cv::Point2f center(m.m10 / m.m00, m.m01 / m.m00);

        if ((r.bottom - center.y) <= 230)
        {
            continue;
        };
        
        circle(windowImage, screenCenter, 3, cv::Scalar(0, 255, 0), -1);

        // std::cout << "Center.y" << center.y << ", " << "R.top" << r.top << std::endl;


        // Remove targets that are too close to the screen edges

        if (
            (center.x <= screenCenter.x + 100 && center.x >= screenCenter.x - 100) &&
            (center.y <= screenCenter.y + 100 && center.y >= screenCenter.y - 100))
        {
            continue;
        };

        monsterCenters.push_back(cv::Point(static_cast<int>(center.x), static_cast<int>(center.y)));
    };

    sort(monsterCenters.begin(), monsterCenters.end(), [&screenCenter](const cv::Point2f &a, const cv::Point2f &b)
         {
        float distA = norm(a - screenCenter);
        float distB = norm(b - screenCenter);
        return distA < distB; });

    out = monsterCenters;

    out = groupedPoints(monsterCenters, 20);

    if (out.size() > maxTargets)
    {
        out.resize(maxTargets);
    };

    if (debug)
    {
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;

        for (int i=0; i<= out.size(); i++)

        {
            cv::Point c = out[i];
            cv::circle(windowImage, c, 2, cv::Scalar(255, 0, 0), -1);
            cv::line(windowImage, c, screenCenter, cv::Scalar(0, 255, 0), 1);
            cv::putText(windowImage, std::to_string(i), c, fontFace, 0.5, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
        };
    };

    return out;
}
void scrollCamera(RECT &r, SerialPort &s, cv::Mat &windowImage)
{

    cv::Point2f screenCenter(windowImage.cols / 2.0f, windowImage.rows / 2.0f);
    int x = static_cast<int>(screenCenter.x + r.left);
    int y = static_cast<int>(screenCenter.y + r.top);

    for (int i = 0; i <= 3; i++)
    {
        moveCursor(x, y, s);
        s.write("swipe_right\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

void pickUp(SerialPort &s)
{
    for (int i = 0; i <= 15; i++)
    {
        s.write("4\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

bool isLowHp(Images &images, SerialPort &s, cv::Mat &windowImage)
{
    cv::Mat cap = capture();
    cv::Rect roi(0, 0, 175, 85);
    cv::Mat searchArea = cap(roi);
    return findTemplate(searchArea, images.lowHpImage, 0.85);
};

bool isTargetHovered(Images &images, cv::Mat &cap)
{
    return findTemplate(cap, images.cursorOnTargetImage);
}

bool isTargetDead(Images &images, cv::Mat &cap)
{
    return findTemplate(cap, images.targetIsDeadImage, 0.95);
};

bool isTargetSelected(Images &images, cv::Mat &cap)
{
    bool isSelected = findTemplate(cap, images.targetSelectedImage);
    return isSelected;
};

bool isTargetAlive(Images &images, cv::Mat &cap)
{
    return findTemplate(cap, images.targetIsAliveImage, 0.9) && !isTargetDead(images, cap);
};

bool isTargetValid(Images &images, cv::Mat &cap)
{
    return isTargetAlive(images, cap);
};

void checkHp(SerialPort &s)
{
    RECT r = getWindowRect();
    HDC dng = GetDC(NULL);
    COLORREF c = GetPixel(dng, 158 + r.left, 78 + r.top);
    ReleaseDC(NULL, dng);
    bool isFull = (int)GetRValue(c) == 181;

    if(!isFull) {
        s.write("5\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };
};


int main()
{
    cv::namedWindow("Window Capture", cv::WINDOW_NORMAL);
    cv::resizeWindow("Window Capture", 1024, 768);
    SerialPort serial;
    cv::Mat windowImage;
    RECT r = getWindowRect();
    Images images;

    while (true)
    {
        std::vector<cv::Point> targets = getTargets(true, windowImage, r);
        std::cout << "Targets count: " << targets.size() << std::endl;
        cv::imshow("Window Capture", windowImage);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (cv::waitKey(200) == 27) { break; };

        cv::Mat cap = capture();

        checkHp(serial);

        bool selected = isTargetSelected(images, cap);
        bool alive = isTargetAlive(images, cap);

        if(selected) {
            if(alive){
                attack(serial);
                std::cout << "Attack!" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                pickUp(serial);
                std::cout << "Pickup!" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        };

        for(int i = 0; i<= targets.size(); i++){
            if(i == targets.size()){
                scrollCamera(r, serial, windowImage);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                break;
            };
            cv::Point t = targets[i];
            moveCursor(t.x + r.left, t.y + r.top, serial);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            bool hovered = isTargetHovered(images, capture());
            bool selected = isTargetSelected(images, capture());

            if(hovered && !selected) {
                mouseClick(serial);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                break;
            };
        };
    };


    cv::destroyAllWindows(); // Закрытие всех окон OpenCV

    return 0;
}
