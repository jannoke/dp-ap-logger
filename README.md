# dp-ap-logger

MySQL/Apache access log logger for datapanel. Reads Apache access logs and stores entries into a MySQL database.

## Dependencies

- `gcc-c++` (g++ compiler)
- `make`
- `mysql-devel` (MySQL client library, provides `-lmysqlclient`)

On Rocky Linux 9 / RHEL 9:
```bash
dnf install -y gcc-c++ make mysql-devel
```

## Build

```bash
make
```

The compiled binary `dp-ap-logger` will be created in the repository root.

To clean build artifacts:
```bash
make clean
```

## Usage

The default configuration file is `/etc/dp-ap-logger/logger.conf`.

Edit `src/config.h` to change the default configuration path or MySQL connection timeouts before building.

## Configuration

See `src/config.h` for compile-time defaults:
- `DEFAULT_CONFIG_FILE` — path to the configuration file
- `MYSQL_CONNECT_TIMEOUT` — MySQL connection timeout in seconds
- `MYSQL_READ_TIMEOUT` — MySQL read timeout in seconds
- `MYSQL_WRITE_TIMEOUT` — MySQL write timeout in seconds

## CI/CD

Automated builds run on Rocky Linux 9 via GitHub Actions on every push to `main` and on pull requests. Tagged releases (`v*`) automatically publish a GitHub Release with the compiled binary.
