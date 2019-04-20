// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";

static char const * const HTTP_200_FORMAT_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\
Set-Cookie: %s\r\n\r\n";

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


bool handle_http_request(int sockfd, int* user);
char* get_cookie(char* buff);
bool write_header_send_file(char* filename, char* buff, char const * format, int sockfd, int n);