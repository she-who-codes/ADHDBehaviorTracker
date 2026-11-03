#include <iostream>

int main() {

    int points = 0;
    int choice = 0;
    
    std::cout << "Current Points:" << points << std::endl;
    
    std::cout << "1> Add Points" << std::endl;
    std::cout << "2> Subtract Points" << std::endl;
    std::cout << "Enter your choice: ";
    std::cin >> choice;

    if (choice == 1) {
        int addPoints = 0;
        std::cout << "Enter points to add: ";
        std::cin >> addPoints;
        points += addPoints;
        std::cout << "New Points: " << points << std::endl;
    } else if (choice == 2) {
        int subtractPoints = 0;
        std::cout << "Enter points to subtract: ";
        std::cin >> subtractPoints;
        points -= subtractPoints;
        std::cout << "New Points: " << points << std::endl;
    } else {
        std::cout << "Invalid choice!" << std::endl;
    }
    return 0;
}