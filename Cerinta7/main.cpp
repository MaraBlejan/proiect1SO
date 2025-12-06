#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono> // Pentru sleep
#include <random> // Pentru generare numere aleatoare
#include <iomanip> // Pentru formatare afisare

// Definim culorile
enum class Color { WHITE, BLACK };

// Helper pentru a converti enum in string sau index
std::string colorToString(Color c) {
    return (c == Color::WHITE) ? "ALB  " : "NEGRU";
}

int colorToIndex(Color c) {
    return (c == Color::WHITE) ? 0 : 1;
}

Color getOpposite(Color c) {
    return (c == Color::WHITE) ? Color::BLACK : Color::WHITE;
}

// ============================================================
//  CLASA MONITOR (Logica de sincronizare)
// ============================================================
class ResourceController {
private:
    std::mutex mtx;
    std::condition_variable cv_white;
    std::condition_variable cv_black;

    int active[2] = {0, 0};  // Cati sunt inauntru
    int waiting[2] = {0, 0}; // Cati asteapta
    Color turn = Color::WHITE; // Pentru a preveni deadlock cand ambele asteapta

public:
    // Logica de intrare (Enter Region)
    void requestAccess(Color myColor) {
        std::unique_lock<std::mutex> lock(mtx);
        
        int me = colorToIndex(myColor);
        int opp = colorToIndex(getOpposite(myColor));

        waiting[me]++;

        /* 
         * LOGICA ANTI-STARVATION SI EXCLUDERE MUTUALA
         * Asteptam daca:
         * 1. Culoarea opusa este activa in resursa (active[opp] > 0).
         * SAU
         * 2. Exista fire de culoare opusa care ASTEAPTA (waiting[opp] > 0)
         *    SI (Eu sunt deja activ cu grupul meu? SAU E randul lor?).
         *    Aceasta clauza forteaza firele curente sa se opreasca din a intra
         *    daca "dusmanul" asteapta la coada.
         */
        auto condition = [&]() {
            bool opponentIsActive = (active[opp] > 0);
            
            // Daca oponentul asteapta, noi ar trebui sa fim politicosi si sa asteptam
            // daca deja ocupam resursa (ca sa o golim) sau daca e randul lor explicit.
            bool starvationProtection = (waiting[opp] > 0) && (active[me] > 0 || turn == getOpposite(myColor));
            
            // Putem intra DOAR DACA nu e activ oponentul SI nu activam protectia la starvation
            return !opponentIsActive && !starvationProtection;
        };

        // Asteptam pe variabila conditionala specifica culorii
        if (myColor == Color::WHITE) {
            cv_white.wait(lock, condition);
        } else {
            cv_black.wait(lock, condition);
        }

        // Am trecut de bariera
        waiting[me]--;
        active[me]++;

        printStatus(myColor, "A INTRAT");
        // Lock-ul se elibereaza automat cand 'lock' este distrus, dar aici il tinem pana iese din scope
    }

    // Logica de iesire (Exit Region)
    void releaseAccess(Color myColor) {
        std::lock_guard<std::mutex> lock(mtx);
        
        int me = colorToIndex(myColor);
        active[me]--;

        printStatus(myColor, "A IESIT ");

        // Daca ultimul fir de culoarea mea a iesit, resursa e libera.
        if (active[me] == 0) {
            // Predam "stafeta" (randul) culorii opuse pentru a evita deadlock-ul
            turn = getOpposite(myColor);

            // Notificam TOATE firele sa verifice din nou conditia.
            // Folosim notify_all pentru ca s-ar putea sa poata intra mai multe de aceeasi culoare.
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

// ============================================================
//  MAIN & THREADS
// ============================================================

ResourceController controller;

void workerThread(Color myColor) {
    // Setup generator numere aleatoare
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> thinkTime(100, 500);
    std::uniform_int_distribution<> workTime(200, 600);

    while (true) {
        // 1. Gandeste (in afara resursei)
        std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime(gen)));

        // 2. Cere acces
        controller.requestAccess(myColor);

        // 3. Utilizeaza resursa (sectiunea critica)
        std::this_thread::sleep_for(std::chrono::milliseconds(workTime(gen)));

        // 4. Elibereaza
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

    // Lansam firele albe
    for (int i = 0; i < NUM_WHITE; ++i) {
        threads.emplace_back(workerThread, Color::WHITE);
    }

    // Lansam firele negre
    for (int i = 0; i < NUM_BLACK; ++i) {
        threads.emplace_back(workerThread, Color::BLACK);
    }

    // Asteptam firele (in acest demo bucla e infinita, deci join nu se va termina niciodata practic)
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}
