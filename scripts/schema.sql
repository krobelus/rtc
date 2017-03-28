CREATE TABLE preprocessing (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,

    commit_id TEXT NOT NULL,
    configuration TEXT,

    time REAL NOT NULL,
    clauses_preprocessed INTEGER NOT NULL,
    removed_clauses_average_length REAL NOT NULL,

    l_can_be_removed INTEGER,
    l_cannot_be_removed INTEGER,

    UNIQUE(name, configuration)
);

CREATE TABLE solving_time (
    name TEXT NOT NULL,
    commit_id TEXT NOT NULL,
    configuration TEXT,

    preprocessed_solving_time_lingeling INTEGER,

    UNIQUE(name, configuration)
);

CREATE TABLE formula (
    name TEXT NOT NULL,
    variables INTEGER NOT NULL,
    clauses INTEGER NOT NULL,
    max_clause_length INTEGER NOT NULL,
    original_solving_time_lingeling INTEGER,
    UNIQUE(name)
);

CREATE TABLE L_sizes (
    name TEXT NOT NULL,
    s1 INTEGER NOT NULL,
    s2 INTEGER NOT NULL,
    s3 INTEGER NOT NULL,
    s4 INTEGER NOT NULL,
    UNIQUE(name)
);
