/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief The Network page: each adapter's state and addresses, and the DNS servers, which can be changed.
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "settings.h"


#define NETWORK_REFRESH_MS  2000
#define NETWORK_IFACES_MAX  8
#define NETWORK_SERVERS_MAX 3
#define NETWORK_RESOLV      "/etc/resolv.conf"
#define NETWORK_ROUTE       "/proc/net/route"
#define NETWORK_RESOLV_MAX  4096


/**
 * @brief The rows of the adapter's card.
 */
typedef enum {

    NETWORK_STATUS = 0,
    NETWORK_ADDRESS,
    NETWORK_MASK,
    NETWORK_GATEWAY,
    NETWORK_HWADDR,

    NETWORK_ROWS,

} network_row_t;


/**
 * @brief The names the adapter's rows are shown under.
 */
static const char* const network_keys[NETWORK_ROWS] = {
    "Status", "IPv4 address", "Subnet mask", "Gateway", "Hardware address",
};


/**
 * @brief The name the DNS card's only row is shown under.
 */
static const char* const network_dns_keys[] = {
    "Servers",
};


/**
 * @brief The page's widgets, the adapters, the one on show, and when they are next read again.
 */
static struct {

    settings_header_t header;

    char names[NETWORK_IFACES_MAX][IF_NAMESIZE];
    ui_widget_t* tiles[NETWORK_IFACES_MAX];
    size_t count;
    size_t selected;

    settings_card_t card;

    ui_widget_t* dns_heading;
    settings_card_t dns;
    ui_widget_t* servers;
    ui_widget_t* save;
    ui_widget_t* status;

    uint64_t next_refresh;

} network;


/**
 * @brief Shows a line under the DNS card, or clears it.
 *
 * @param text The line, or "" for none.
 */
static void network_report(const char* text) {
    settings_fit(network.status, text);
}


/**
 * @brief Formats an IPv4 address held in network order.
 *
 * @param addr The address.
 * @param out Receives the text.
 * @param size The size of @p out.
 */
static void network_format_ip(uint32_t addr, char* out, size_t size) {

    struct in_addr in = {.s_addr = addr};

    if (!inet_ntop(AF_INET, &in, out, (socklen_t)size)) {
        snprintf(out, size, SETTINGS_UNKNOWN);
    }
}


/**
 * @brief Reads one IPv4 address of an interface.
 *
 * @param fd A socket to ask on.
 * @param name The interface.
 * @param request SIOCGIFADDR or SIOCGIFNETMASK.
 * @param out Receives the address, in network order.
 * @return 0, or -1 with errno set.
 */
static int network_ioctl_ip(int fd, const char* name, unsigned long request, uint32_t* out) {

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);

    if (ioctl(fd, request, &ifr) < 0) {
        return -1;
    }

    *out = ((const struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;

    return 0;
}


/**
 * @brief Finds the default gateway of an interface in /proc/net/route.
 *
 * @param name The interface.
 * @param out Receives the gateway, in network order.
 * @return 0, or -1 when the interface has no default route.
 */
static int network_gateway(const char* name, uint32_t* out) {

    FILE* fp = fopen(NETWORK_ROUTE, "r");

    if (!fp) {
        return -1;
    }

    char line[256];
    int found = -1;

    while (found < 0 && fgets(line, sizeof(line), fp)) {

        char iface[IF_NAMESIZE + 1];
        unsigned long destination;
        unsigned long gateway;
        unsigned flags;

        if (sscanf(line, "%16s %lx %lx %x", iface, &destination, &gateway, &flags) != 4) {
            continue;
        }

        if (strcmp(iface, name) == 0 && destination == 0 && (flags & 0x2)) {
            *out  = (uint32_t)gateway;
            found = 0;
        }
    }

    fclose(fp);

    return found;
}


/**
 * @brief Lists the adapters, leaving the loopback out.
 */
static void network_scan(void) {

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        return;
    }

    struct ifreq reqs[NETWORK_IFACES_MAX];
    struct ifconf conf = {.ifc_len = sizeof(reqs), .ifc_req = reqs};

    if (ioctl(fd, SIOCGIFCONF, &conf) == 0) {

        const size_t count = (size_t)conf.ifc_len / sizeof(reqs[0]);

        for (size_t i = 0; i < count && network.count < NETWORK_IFACES_MAX; i++) {

            struct ifreq ifr = reqs[i];

            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0 && (ifr.ifr_flags & IFF_LOOPBACK)) {
                continue;
            }

            snprintf(network.names[network.count++], IF_NAMESIZE, "%.*s", IF_NAMESIZE - 1, reqs[i].ifr_name);
        }
    }

    close(fd);
}


/**
 * @brief Reads the adapter on show into the card.
 */
static void network_refresh(void) {

    settings_row_t* rows = network.card.rows;

    if (network.count == 0) {

        settings_row_set(&rows[NETWORK_STATUS], "No network adapter");

        for (size_t i = NETWORK_ADDRESS; i < NETWORK_ROWS; i++) {
            settings_row_set(&rows[i], "\xE2\x80\x94");
        }

        return;
    }


    const char* name = network.names[network.selected];

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        return;
    }

    char text[SETTINGS_TEXT_MAX];

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {

        const char* state = !(ifr.ifr_flags & IFF_UP) ? "Off" : (ifr.ifr_flags & IFF_RUNNING) ? "Connected" : "Disconnected";

        snprintf(text, sizeof(text), "%s (%s)", state, name);
        settings_row_set(&rows[NETWORK_STATUS], text);
    }


    uint32_t addr = 0;
    uint32_t mask = 0;

    if (network_ioctl_ip(fd, name, SIOCGIFADDR, &addr) == 0 && addr != 0) {
        network_format_ip(addr, text, sizeof(text));
        settings_row_set(&rows[NETWORK_ADDRESS], text);
    } else {
        settings_row_set(&rows[NETWORK_ADDRESS], "Not assigned");
    }

    if (network_ioctl_ip(fd, name, SIOCGIFNETMASK, &mask) == 0 && mask != 0) {
        network_format_ip(mask, text, sizeof(text));
        settings_row_set(&rows[NETWORK_MASK], text);
    } else {
        settings_row_set(&rows[NETWORK_MASK], "\xE2\x80\x94");
    }


    uint32_t gateway;

    if (network_gateway(name, &gateway) == 0) {
        network_format_ip(gateway, text, sizeof(text));
        settings_row_set(&rows[NETWORK_GATEWAY], text);
    } else {
        settings_row_set(&rows[NETWORK_GATEWAY], "None");
    }


    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {

        const unsigned char* hw = (const unsigned char*)ifr.ifr_hwaddr.sa_data;

        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5]);
        settings_row_set(&rows[NETWORK_HWADDR], text);
    }

    close(fd);
}


/**
 * @brief Reads the DNS servers out of NETWORK_RESOLV into the field, comma-separated.
 */
static void network_read_servers(void) {

    char text[SETTINGS_TEXT_MAX] = "";

    FILE* fp = fopen(NETWORK_RESOLV, "r");

    if (fp) {

        char line[256];

        while (fgets(line, sizeof(line), fp)) {

            char server[INET6_ADDRSTRLEN + 1];

            if (sscanf(line, " nameserver %46s", server) != 1) {
                continue;
            }

            const size_t length = strlen(text);

            snprintf(text + length, sizeof(text) - length, "%s%s", length ? ", " : "", server);
        }

        fclose(fp);
    }

    ui_entry_set_text(network.servers, text);
}


/**
 * @brief Writes the servers the field names into NETWORK_RESOLV, keeping every line that is not a nameserver.
 *
 * @param widget The Save button or the field.
 * @param user Unused.
 */
static void network_on_save(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    char list[SETTINGS_TEXT_MAX];
    char servers[NETWORK_SERVERS_MAX][INET6_ADDRSTRLEN];
    char text[SETTINGS_TEXT_MAX];

    size_t count = 0;

    snprintf(list, sizeof(list), "%s", ui_entry_text(network.servers));

    for (char* token = strtok(list, ", \t"); token; token = strtok(NULL, ", \t")) {

        unsigned char addr[sizeof(struct in6_addr)];

        if (inet_pton(AF_INET, token, addr) != 1 && inet_pton(AF_INET6, token, addr) != 1) {
            snprintf(text, sizeof(text), "Not an IP address: %s", token);
            network_report(text);
            return;
        }

        if (count == NETWORK_SERVERS_MAX) {
            network_report("Up to three servers are used.");
            return;
        }

        snprintf(servers[count++], sizeof(servers[0]), "%s", token);
    }

    if (count == 0) {
        network_report("Name at least one server, such as 1.1.1.1.");
        return;
    }


    static char contents[NETWORK_RESOLV_MAX];

    size_t length = 0;

    FILE* fp = fopen(NETWORK_RESOLV, "r");

    if (fp) {

        char line[256];

        while (fgets(line, sizeof(line), fp)) {

            char word[16];

            if (sscanf(line, " %15s", word) == 1 && strcmp(word, "nameserver") == 0) {
                continue;
            }

            length += (size_t)snprintf(contents + length, sizeof(contents) - length, "%s", line);

            if (length >= sizeof(contents)) {
                fclose(fp);
                network_report("The resolver configuration is too long to rewrite.");
                return;
            }
        }

        fclose(fp);
    }

    for (size_t i = 0; i < count && length < sizeof(contents); i++) {
        length += (size_t)snprintf(contents + length, sizeof(contents) - length, "nameserver %s\n", servers[i]);
    }

    if (length >= sizeof(contents) || settings_write_file(NETWORK_RESOLV, contents) < 0) {
        snprintf(text, sizeof(text), "Cannot save the servers: %s", length >= sizeof(contents) ? "too long" : strerror(errno));
        network_report(text);
        return;
    }

    network_read_servers();
    network_report("Saved. New lookups use these servers.");
}


/**
 * @brief Shows the adapter a tile names.
 *
 * @param widget The tile.
 * @param user The adapter's index.
 */
static void network_on_tile(ui_widget_t* widget, void* user) {

    (void)widget;

    network.selected = (size_t)(intptr_t)user;

    for (size_t i = 0; i < network.count; i++) {
        ui_button_set_style(network.tiles[i], i == network.selected ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_DEFAULT);
    }

    network_refresh();
}


/**
 * @brief Creates the page's widgets, with a tile per adapter when there is more than one.
 *
 * @return 0 on success, -1 otherwise.
 */
static int network_build(void) {

    if (settings_header_build(&network.header, "Network", "The addresses this system uses, and the servers that turn names into them.") < 0) {
        return -1;
    }

    network_scan();

    for (size_t i = 0; network.count > 1 && i < network.count; i++) {

        if ((network.tiles[i] = settings_own(ui_button_create(settings.view, network.names[i], network_on_tile, (void*)(intptr_t)i))) == NULL) {
            return -1;
        }
    }

    if (settings_card_build(&network.card, network_keys, NETWORK_ROWS, 0) < 0) {
        return -1;
    }

    network.dns_heading = settings_heading("DNS");

    if (!network.dns_heading || settings_card_build(&network.dns, network_dns_keys, 1, 1U) < 0) {
        return -1;
    }

    network.servers = settings_own(ui_entry_create(settings.view, "1.1.1.1, 1.0.0.1"));
    network.save    = settings_own(ui_button_create(settings.view, "Save", network_on_save, NULL));
    network.status  = settings_own(ui_label_create(settings.view, ""));

    if (!network.servers || !network.save || !network.status) {
        return -1;
    }

    ui_entry_on_submit(network.servers, network_on_save, NULL);

    if (network.count > 1) {
        network_on_tile(NULL, (void*)(intptr_t)0);
    }

    return 0;
}


/**
 * @brief Lays out the adapter tiles, the adapter's card, and the DNS card with its field.
 *
 * @param page The page's rectangle.
 */
static void network_layout(ui_rect_t page) {

    const int control = settings_control_height();

    int y = settings_header_place(&network.header, page, 0);

    if (network.count > 1) {

        const int columns = network.count < 4 ? (int)network.count : 4;

        ui_grid_t grid = ui_grid((ui_rect_t){page.x, y, page.width, control}, columns, 1, SETTINGS_GAP);

        for (size_t i = 0; i < network.count && (int)i < columns; i++) {
            ui_widget_place(network.tiles[i], ui_grid_cell(&grid, (int)i, 0, 1, 1));
        }

        y += control + SETTINGS_GAP * 2;
    }

    y += settings_card_place(&network.card, page.x, y, page.width) + SETTINGS_SECTION_GAP;

    ui_widget_set_rect(network.dns_heading, page.x, y, page.width, control);

    y += control + SETTINGS_GAP;
    y += settings_card_place(&network.dns, page.x, y, page.width);


    const ui_rect_t value = settings_card_value(&network.dns, 0);
    const int button      = settings_text_width("Save", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;
    const int field       = value.width - button - SETTINGS_GAP;

    ui_widget_set_rect(network.servers, value.x, value.y + 5, field > 0 ? field : 0, value.height - 10);
    ui_widget_set_rect(network.save, value.x + value.width - button, value.y + 5, button, value.height - 10);

    ui_widget_set_rect(network.status, page.x, y + SETTINGS_GAP, page.width, settings_line_height());
}


/**
 * @brief Gives the page the colours of the theme in use.
 */
static void network_restyle(void) {

    settings_header_restyle(&network.header);
    settings_card_restyle(&network.card);
    settings_card_restyle(&network.dns);

    ui_label_set_color(network.status, ui_theme()->text_muted);
}


/**
 * @brief Fills the field from NETWORK_RESOLV and reads the adapter straight away.
 */
static void network_enter(void) {

    network_read_servers();
    network_report("");

    network.next_refresh = 0;
}


/**
 * @brief Reads the adapter again every two seconds while the page is on show, for a cable pulled or plugged.
 *
 * @param now The clock, in milliseconds.
 * @param visible Whether the page is on show.
 * @return How long until the next read, or -1 while hidden.
 */
static int network_tick(uint64_t now, bool visible) {

    if (!visible) {
        return -1;
    }

    if (now >= network.next_refresh) {
        network_refresh();
        network.next_refresh = now + NETWORK_REFRESH_MS;
    }

    return (int)(network.next_refresh - now);
}


/**
 * @brief The Network page.
 */
const settings_page_t settings_network = {
    .name    = "network",
    .label   = "Network",
    .icon    = "network-wired",
    .build   = network_build,
    .layout  = network_layout,
    .restyle = network_restyle,
    .enter   = network_enter,
    .tick    = network_tick,
};
