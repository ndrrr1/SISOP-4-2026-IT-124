#!/bin/bash
set -e

MODE="${1:-server}"

LOG_DIR="/var/log/libraryit"
RAW_LOG="$LOG_DIR/audit_raw.log"
FINAL_LOG="$LOG_DIR/libraryit.log"

mkdir -p "$LOG_DIR"
touch "$RAW_LOG" "$FINAL_LOG"
chmod 666 "$RAW_LOG" "$FINAL_LOG"

if [ "$MODE" = "logger" ]; then
    tail -n +1 -F "$RAW_LOG" | while read -r line; do
        case "$line" in
            *"smbd_audit:"*)
                payload="${line#*smbd_audit: }"

                username="$(echo "$payload" | cut -d'|' -f1)"
                share="$(echo "$payload" | cut -d'|' -f2)"
                action="$(echo "$payload" | cut -d'|' -f3)"
                result="$(echo "$payload" | cut -d'|' -f4)"
                target="$(echo "$payload" | cut -d'|' -f5-)"

                [ -z "$username" ] && username="-"
                [ -z "$share" ] && share="-"
                [ -z "$action" ] && action="-"
                [ -z "$target" ] && target="$share"

                upper_action="$(echo "$action" | tr '[:lower:]' '[:upper:]')"

                if echo "$result" | grep -qi "fail"; then
                    level="WARNING"
                    upper_action="DENIED"
                else
                    level="INFO"
                fi

                timestamp="$(date '+%Y-%m-%d %H:%M:%S')"
                formatted="[$timestamp] [$level] [$username] [$upper_action] [$target]"

                echo "$formatted" | tee -a "$FINAL_LOG"
                ;;
        esac
    done
    exit 0
fi

groupadd -f readonly
groupadd -f staff

id member >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g readonly member
id contributor >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g staff contributor
id librarian >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin -g staff librarian

rm -f /etc/samba/smbpasswd
touch /etc/samba/smbpasswd
chmod 600 /etc/samba/smbpasswd

printf "member123\nmember123\n" | smbpasswd -s -a member
printf "contrib456\ncontrib456\n" | smbpasswd -s -a contributor
printf "lib789\nlib789\n" | smbpasswd -s -a librarian

smbpasswd -e member
smbpasswd -e contributor
smbpasswd -e librarian

mkdir -p /libraryit/docs /libraryit/ebooks /libraryit/papers /libraryit/sourcecode

chown -R librarian:staff /libraryit/docs
chmod -R 775 /libraryit/docs

chown -R root:staff /libraryit/ebooks
chmod -R 2775 /libraryit/ebooks

chown -R root:staff /libraryit/papers
chmod -R 2775 /libraryit/papers

chown -R root:staff /libraryit/sourcecode
chmod -R 2770 /libraryit/sourcecode

cat > /etc/rsyslog.conf <<RSYSLOG
module(load="imuxsock")
local7.*    $RAW_LOG
*.*         /dev/null
RSYSLOG

rm -f /run/rsyslogd.pid
rsyslogd

logger -p local7.notice "smbd_audit: system|startup|connect|ok|libraryit"

testparm -s /etc/samba/smb.conf

exec smbd --foreground --no-process-group --debug-stdout
