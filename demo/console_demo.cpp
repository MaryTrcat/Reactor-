#include "business/FileUserRepository.h"
#include "business/UserService.h"

#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace {

void clearInput() {
    std::cin.clear();
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
}

std::string readText(const std::string& label) {
    std::cout << label;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

void registerUser(business::UserService& service) {
    const auto username = readText("username: ");
    const auto password = readText("password: ");
    const auto role = readText("role(user/admin): ");

    const auto result = service.registerUser(username, password, role);
    std::cout << (result.success ? "OK" : "FAIL") << ": " << result.message;
    if (result.success) {
        std::cout << ", user_id=" << result.user_id;
    }
    std::cout << "\n";
}

void login(business::UserService& service) {
    const auto username = readText("username: ");
    const auto password = readText("password: ");

    const auto result = service.login(username, password);
    std::cout << (result.success ? "OK" : "FAIL") << ": " << result.message;
    if (result.success) {
        std::cout << ", user_id=" << result.user_id << ", role=" << result.role;
    }
    std::cout << "\n";
}

void queryUser(business::UserService& service) {
    const auto username = readText("username: ");

    const auto result = service.queryUser(username);
    std::cout << (result.found ? "FOUND" : "NOT FOUND") << ": " << result.message;
    if (result.found) {
        std::cout << ", user_id=" << result.user_id << ", username=" << result.username << ", role=" << result.role;
    }
    std::cout << "\n";
}

void checkPermission(business::UserService& service) {
    const auto username = readText("username: ");
    const auto required_role = readText("required role(user/admin): ");

    const auto result = service.checkPermission(username, required_role);
    std::cout << (result.allowed ? "ALLOWED" : "DENIED") << ": " << result.message << "\n";
}

void changePassword(business::UserService& service) {
    const auto username = readText("username: ");
    const auto new_password = readText("new password: ");

    const auto result = service.changePassword(username, new_password);
    std::cout << (result.success ? "OK" : "FAIL") << ": " << result.message << "\n";
}

void deleteUser(business::UserService& service) {
    const auto username = readText("username: ");

    const auto result = service.deleteUser(username);
    std::cout << (result.success ? "OK" : "FAIL") << ": " << result.message << "\n";
}

void listUsers(business::UserService& service) {
    const auto users = service.listUsers();
    if (users.empty()) {
        std::cout << "no users\n";
        return;
    }

    for (const auto& user : users) {
        std::cout << "user_id=" << user.id << ", username=" << user.username << ", role=" << user.role << "\n";
    }
}

void printMenu() {
    std::cout << "\n=== Reactor User Service Console Demo ===\n"
              << "1. Register user\n"
              << "2. Login\n"
              << "3. Query user\n"
              << "4. Check permission\n"
              << "5. Change password\n"
              << "6. Delete user\n"
              << "7. List users\n"
              << "0. Exit\n"
              << "choice: ";
}

}  // namespace

int main() {
    auto repository = std::make_shared<business::FileUserRepository>("data/users.txt");
    business::UserService service(repository);

    while (true) {
        printMenu();

        int choice = -1;
        if (!(std::cin >> choice)) {
            clearInput();
            std::cout << "invalid choice\n";
            continue;
        }
        clearInput();

        switch (choice) {
            case 1:
                registerUser(service);
                break;
            case 2:
                login(service);
                break;
            case 3:
                queryUser(service);
                break;
            case 4:
                checkPermission(service);
                break;
            case 5:
                changePassword(service);
                break;
            case 6:
                deleteUser(service);
                break;
            case 7:
                listUsers(service);
                break;
            case 0:
                std::cout << "bye\n";
                return 0;
            default:
                std::cout << "invalid choice\n";
                break;
        }
    }
}
