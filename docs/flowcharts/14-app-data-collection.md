# Data Collection & Analysis (Central Computer / PC app)

Keeps a structured record store (flat-file, documented stand-in for a
real database) and answers simple report queries over it.

```mermaid
graph TD
  A[New measurement / event received] --> B[DataStore: append structured record]
  B --> C[Flat CSV-style file on disk]
  D[Caller asks for a report] --> E[DataCollection: read matching records]
  E --> F[Summarize / return breakdown]
```

Source: `data_collection.h/.cpp`, `data_store.h/.cpp`.
