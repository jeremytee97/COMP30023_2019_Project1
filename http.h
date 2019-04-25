// constants
#define MAX_SIZE_OF_KEYWORD 101
#define MAX_KEYWORD_NUM 20
#define MAX_COOKIE 10
#define NUM_USER 2


static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";

static char const * const HTTP_200_FORMAT_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\
Set-Cookie: %d\r\n\r\n";

static char const * const PARAGRAPH = "<p>%s</p>";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

// represents the types of method
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;


bool handle_http_request(int sockfd, int* user, char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD], 
    char user_cookie_mapping[][MAX_SIZE_OF_KEYWORD], int* current_players_cookie);

int get_cookie(char* buff);

bool write_header_send_file(char* filename, char* buff, char const * format, int sockfd, int n);

int next_guess_num(char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD], int cookie);

int next_player_num(int current_players_cookie[]);

int get_opponent_cookie(int current_players_cookie[], int user_cookie);

void reinitialise_player_state_and_guesses(int state[], int current_players_cookie[], 
    char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]);

void register_player(int cookie, int current_player_cookies[]);

void initialise_guesses(char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]);