#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_USERS 100
#define MAX_LOGIN_LEN 6
#define MAX_COMMAND_LEN 100
#define ADMIN_PIN 12345

typedef struct {
    char login[MAX_LOGIN_LEN + 1];
    int pin;
    int request_limit;
    int request_count;
    bool is_active;
    bool is_blocked;
} User;

User users[MAX_USERS];
int user_count = 0;
User *current_user = NULL;

void init_users() {
    strcpy(users[0].login, "admin");
    users[0].pin = 1234;
    users[0].request_limit = -1;
    users[0].request_count = 0;
    users[0].is_active = true;
    users[0].is_blocked = false;
    user_count = 1;
}

User* find_user(const char *login) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].login, login) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

bool validate_login(const char *login) {
    if (strlen(login) > MAX_LOGIN_LEN) return false;

    for (int i = 0; login[i]; i++) {
        if (!isalnum(login[i])) {
            return false;
        }
    }
    return true;
}

bool validate_pin(int pin) {
    return pin >= 0 && pin <= 100000;
}

void register_user() {
    char login[MAX_LOGIN_LEN + 1];
    int pin;
    char line[256];

    printf("Регистрация нового пользователя\n");

    while (1) {
        printf("Введите логин (макс. %d символов, буквы и цифры): ", MAX_LOGIN_LEN);
        fgets(line, sizeof(line), stdin);
        line[strcspn(line, "\n")] = '\0';

        if (!validate_login(line)) {
            printf("Недопустимый логин. Попробуйте еще раз.\n");
            continue;
        }

        if (find_user(line) != NULL) {
            printf("Пользователь с таким логином уже существует.\n");
            continue;
        }

        strncpy(login, line, MAX_LOGIN_LEN);
        login[MAX_LOGIN_LEN] = '\0';
        break;
    }

    while (1) {
        printf("Введите PIN-код (0-100000): ");
        fgets(line, sizeof(line), stdin);

        if (sscanf(line, "%d", &pin) != 1 || !validate_pin(pin)) {
            printf("Недопустимый PIN-код. Попробуйте еще раз.\n");
            continue;
        }

        break;
    }

    strcpy(users[user_count].login, login);
    users[user_count].pin = pin;
    users[user_count].request_limit = -1;
    users[user_count].request_count = 0;
    users[user_count].is_active = true;
    users[user_count].is_blocked = false;
    user_count++;

    printf("Пользователь %s успешно зарегистрирован.\n", login);
}

void show_time() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("Текущее время: %02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void show_date() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("Текущая дата: %02d.%02d.%d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
}

void logout() {
    if (current_user != NULL) {
        current_user->is_blocked = false;
        current_user->request_count = 0;
    }
    current_user = NULL;
    printf("Вы вышли из системы.\n");
}

void set_sanctions(const char *username, int limit) {
    int confirm;
    printf("Для подтверждения введите 12345: ");
    if (scanf("%d", &confirm) != 1 || confirm != ADMIN_PIN) {
        printf("Неверный код подтверждения.\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    User *user = find_user(username);
    if (user == NULL) {
        printf("Пользователь %s не найден.\n", username);
        return;
    }

    user->request_limit = limit;
    user->request_count = 0;
    user->is_blocked = false;
    printf("Для пользователя %s установлен лимит %d запросов.\n", username, limit);
}

bool login_user() {
    char login[MAX_LOGIN_LEN + 1];
    int pin;
    char line[256];

    printf("Авторизация пользователя\n");
    printf("Введите логин: ");
    fgets(line, sizeof(line), stdin);
    line[strcspn(line, "\n")] = '\0';
    strncpy(login, line, MAX_LOGIN_LEN);

    printf("Введите PIN-код: ");
    fgets(line, sizeof(line), stdin);
    sscanf(line, "%d", &pin);

    User *user = find_user(login);
    if (user == NULL || user->pin != pin || !user->is_active) {
        printf("Ошибка авторизации.\n");
        return false;
    }

    user->request_count = 0;
    user->is_blocked = false;
    current_user = user;
    printf("Добро пожаловать, %s!\n", login);
    return true;
}

void process_command() {
    char line[MAX_COMMAND_LEN];

    printf("%s> ", current_user->login);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return;
    }

    line[strcspn(line, "\n")] = '\0';

    if (current_user->is_blocked) {
        if (strcmp(line, "logout") == 0 || strcmp(line, "Logout") == 0) {
            logout();
        } else {
            printf("Лимит запросов (%d) исчерпан. Введите 'logout' для выхода.\n",
                  current_user->request_limit);
        }
        return;
    }

    if (current_user->request_limit > 0 &&
        current_user->request_count >= current_user->request_limit) {
        printf("Превышен лимит запросов (%d). Вы больше не можете выполнять команды.\n",
              current_user->request_limit);
        current_user->is_blocked = true;
        return;
    }

    if (strcmp(line, "Time") == 0) {
        show_time();
        current_user->request_count++;
    }
    else if (strcmp(line, "Date") == 0) {
        show_date();
        current_user->request_count++;
    }
    else if (strncmp(line, "Howmuch ", 8) == 0) {
        current_user->request_count++;
    }
    else if (strcmp(line, "Logout") == 0 || strcmp(line, "logout") == 0) {
        logout();
    }
    else if (strncmp(line, "Sanctions ", 10) == 0) {
        char username[MAX_LOGIN_LEN + 1];
        int limit;
        if (sscanf(line + 10, "%6s %d", username, &limit) == 2) {
            set_sanctions(username, limit);
            current_user->request_count++;
        }
    }
    else {
        printf("Неизвестная команда. Доступные команды:\n");
        printf("Time - текущее время\n");
        printf("Date - текущая дата\n");
        printf("Howmuch <дата> <флаг> - прошедшее время\n");
        printf("Logout - выход\n");
        printf("Sanctions <логин> <число> - установить ограничения\n");
    }
}

void show_auth_menu() {
    int choice;
    char line[256];

    while (1) {
        printf("\nМеню авторизации\n");
        printf("1. Вход\n");
        printf("2. Регистрация\n");
        printf("3. Выход\n");
        printf("Выберите действие: ");

        fgets(line, sizeof(line), stdin);
        if (sscanf(line, "%d", &choice) != 1) {
            continue;
        }

        switch (choice) {
            case 1:
                if (login_user()) return;
                break;
            case 2:
                register_user();
                break;
            case 3:
                exit(0);
            default:
                printf("Неверный выбор. Попробуйте еще раз.\n");
        }
    }
}

int main() {
    init_users();

    while (1) {
        show_auth_menu();

        while (current_user != NULL) {
            process_command();
        }
    }
    
    return 0;
}