#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono> 
#include <random> 
#include <iomanip> 


enum class Color { WHITE, BLACK };


std::string colorToString(Color c) {
    return (c == Color::WHITE) ? "ALB  " : "NEGRU";
}

int colorToIndex(Color c) {
    return (c == Color::WHITE) ? 0 : 1;
}

Color getOpposite(Color c) {
    return (c == Color::WHITE) ? Color::BLACK : Color::WHITE;
}


class ResourceController {
private:
    std::mutex mtx;
    std::condition_variable cv_white;
    std::condition_variable cv_black;

    int active[2] = {0, 0};  
    int waiting[2] = {0, 0};
    Color turn = Color::WHITE; 

public:
   
    void requestAccess(Color myColor) {
        std::unique_lock<std::mutex> lock(mtx);
        
        int me = colorToIndex(myColor);
        int opp = colorToIndex(getOpposite(myColor));

        waiting[me]++;

        auto condition = [&]() {
            bool opponentIsActive = (active[opp] > 0);
            
            
            bool starvationProtection = (waiting[opp] > 0) && (active[me] > 0 || turn == getOpposite(myColor));
            
           
            return !opponentIsActive && !starvationProtection;
        };

    
        if (myColor == Color::WHITE) {
            cv_white.wait(lock, condition);
        } else {
            cv_black.wait(lock, condition);
        }

       
        waiting[me]--;
        active[me]++;

        printStatus(myColor, "A INTRAT");
       
    }

    
    void releaseAccess(Color myColor) {
        std::lock_guard<std::mutex> lock(mtx);
        
        int me = colorToIndex(myColor);
        active[me]--;

        printStatus(myColor, "A IESIT ");

     
        if (active[me] == 0) {
           
            turn = getOpposite(myColor);

           
            cv_white.notify_all();
            cv_black.notify_all();
        }
    }

private:
    void printStatus(Color c, const std::string& action) {
        std::cout << "[" << colorToString(c) << "] " << action 
                  << " | Active: (A:" << active[0] << ", N:" << active[1] << ")"
                  << " Waiting: (A:" << waiting[0] << ", N:" << waiting[1] << ")"
                  << std::endl;
    }
};



ResourceController controller;

void workerThread(Color myColor) {
 
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> thinkTime(100, 500);
    std::uniform_int_distribution<> workTime(200, 600);

    while (true) {
     
        std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime(gen)));

    
        controller.requestAccess(myColor);

      
        std::this_thread::sleep_for(std::chrono::milliseconds(workTime(gen)));

     
        controller.releaseAccess(myColor);
    }
}

int main() {
    std::cout << "Start aplicatie C++ White-Black Threads..." << std::endl;
    
    #ifdef _WIN32
        std::cout << "Sistem detectat: Windows" << std::endl;
    #else
        std::cout << "Sistem detectat: Linux" << std::endl;
    #endif

    const int NUM_WHITE = 5;
    const int NUM_BLACK = 5;
    std::vector<std::thread> threads;


    for (int i = 0; i < NUM_WHITE; ++i) {
        threads.emplace_back(workerThread, Color::WHITE);
    }

    
    for (int i = 0; i < NUM_BLACK; ++i) {
        threads.emplace_back(workerThread, Color::BLACK);
    }

 
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}
