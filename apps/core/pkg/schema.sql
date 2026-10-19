PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS repos (
    name     TEXT PRIMARY KEY,
    url      TEXT NOT NULL,
    priority INTEGER NOT NULL,
    updated  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS remote (
    repo    TEXT NOT NULL REFERENCES repos (name) ON DELETE CASCADE,
    name    TEXT NOT NULL,
    version TEXT NOT NULL,
    sha256  TEXT NOT NULL,
    PRIMARY KEY (repo, name)
);

CREATE TABLE IF NOT EXISTS remote_deps (
    repo TEXT NOT NULL,
    name TEXT NOT NULL,
    dep  TEXT NOT NULL,
    PRIMARY KEY (repo, name, dep),
    FOREIGN KEY (repo, name) REFERENCES remote (repo, name) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS installed (
    name     TEXT PRIMARY KEY,
    version  TEXT NOT NULL,
    repo     TEXT,
    sha256   TEXT,
    explicit INTEGER NOT NULL DEFAULT 1,
    date     INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS installed_deps (
    name TEXT NOT NULL REFERENCES installed (name) ON DELETE CASCADE,
    dep  TEXT NOT NULL,
    PRIMARY KEY (name, dep)
);

CREATE TABLE IF NOT EXISTS files (
    package TEXT NOT NULL REFERENCES installed (name) ON DELETE CASCADE,
    path    TEXT NOT NULL,
    type    TEXT NOT NULL CHECK (type IN ('f', 'l', 'd')),
    PRIMARY KEY (package, path)
);

CREATE INDEX IF NOT EXISTS files_path ON files (path);
CREATE INDEX IF NOT EXISTS installed_deps_dep ON installed_deps (dep);

CREATE VIEW IF NOT EXISTS candidates AS
    SELECT r.repo, r.name, r.version, r.sha256, p.url
    FROM remote r
    JOIN repos p ON p.name = r.repo
    WHERE p.priority = (
        SELECT MIN(q.priority) FROM remote s JOIN repos q ON q.name = s.repo WHERE s.name = r.name
    );
