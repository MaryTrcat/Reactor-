#include "business/FileUserRepository.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "reactor_user_service_file_repository_test.txt";
    std::filesystem::remove(path);

    {
        business::FileUserRepository repository(path.string());

        std::int64_t alice_id = 0;
        std::string error;
        assert(repository.createUser("alice", "hash1", "admin", alice_id, error));
        assert(alice_id == 1);

        std::int64_t duplicate_id = 0;
        assert(!repository.createUser("alice", "hash1", "admin", duplicate_id, error));

        std::int64_t bob_id = 0;
        assert(repository.createUser("bob", "hash2", "user", bob_id, error));
        assert(bob_id == 2);

        assert(repository.updateStatus("bob", true, error));

        const auto users = repository.listUsers();
        assert(users.size() == 2);
        assert(users[0].username == "alice");
        assert(users[1].username == "bob");
    }

    {
        business::FileUserRepository repository(path.string());

        const auto alice = repository.findByUsername("alice");
        assert(alice.has_value());
        assert(alice->id == 1);
        assert(alice->password_hash == "hash1");
        assert(alice->role == "admin");

        const auto bob = repository.findByUsername("bob");
        assert(bob.has_value());
        assert(bob->frozen);

        std::string error;
        assert(repository.updatePassword("alice", "hash3", error));
        assert(!repository.updatePassword("missing", "hash4", error));
        assert(repository.deleteUser("bob", error));
        assert(!repository.deleteUser("missing", error));
    }

    {
        business::FileUserRepository repository(path.string());

        const auto alice = repository.findByUsername("alice");
        assert(alice.has_value());
        assert(alice->password_hash == "hash3");

        const auto bob = repository.findByUsername("bob");
        assert(!bob.has_value());

        const auto users = repository.listUsers();
        assert(users.size() == 1);
        assert(users[0].username == "alice");
    }

    std::filesystem::remove(path);
    std::cout << "test_file_user_repository passed\n";
    return 0;
}
