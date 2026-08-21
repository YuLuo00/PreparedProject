# CoserRetrieval Architecture

`CoserRetrieval` is the active image retrieval system. It is a native C++17
library plus the `coser_cli` command-line entry point. The old ImageSearch DLL
and Python services at the repository root are historical code and are not a
dependency of this system.

## System Map

```text
                         +-----------------------+
                         | coser_cli             |
                         | ingest/query/scan/... |
                         +-----------+-----------+
                                     |
                     +---------------v----------------+
                     | L5 RetrievalOrchestrator         |
                     | route selection + result fusion  |
                     +---+----------------+-------------+
                         |                |
          +--------------v---+   +--------v------------------+
          | L4 Face Pipeline |   | L4 Clothing / Image Route  |
          +--------+---------+   +--------+-------------------+
                   |                      |
     +-------------v----------------------v-------------+
     | L3 model adapters                              |
     | SCRFD | ArcFace/AdaFace | YOLO Pose | LIP      |
     | DINOv2 | CLIP | pHash | ORB                      |
     +--------------------+----------------------------+
                          |
            +-------------v-------------+
            | L2 persistent/search data |
            | SQLite | Faiss | pHash    |
            +-------------+-------------+
                          |
            +-------------v-------------+
            | L1 infrastructure         |
            | MD5 | config | progress   |
            +---------------------------+
```

## Layer Responsibilities

| Layer | Purpose | Main types |
|---|---|---|
| L1 | Stateless/shared infrastructure | `FileHasher`, `ConfigManager`, `TaskProgressHub` |
| L2 | Persistence and indices | `MetadataStore`, `FaissFlatIpIndex`, `PHashIndex` |
| L3 | Concrete model or image-algorithm adapters | `ScrfdFaceDetector`, `ArcFaceExtractor`, `HumanParsingSegmenter`, `DinoV2Extractor`, `PHasher`, `OrbCropMatcher` |
| L4 | One retrieval route per visual signal | `FaceRecognitionPipeline`, `ClothingRecognitionPipeline`, `ImageMatchPipeline` |
| L5 | Coordinates L4 routes and combines results | `RetrievalOrchestrator`, `ResultFusion` |

The interfaces `IDetector`, `IPoseDetector`, `IEmbeddingExtractor`, and
`IVectorIndex` keep L4/L5 independent from a particular ONNX model or Faiss
implementation.

## Retrieval Routes

```text
Face route
image -> SCRFD -> face alignment -> ArcFace (or AdaFace) -> Faiss IP -> person

Clothing / role route
image -> YOLO Pose person box -> LIP parsing -> semantic mask
      -> DINOv2 patch tokens -> mask-weighted pooling -> Faiss IP -> person/role

Exact-image route
image -> pHash candidate lookup -> ORB geometric verification -> original image
```

The clothing route keeps only LIP classes for hair and wearable apparel. Face,
skin, limbs, and background are excluded. The mask is downsampled to DINOv2's
`16 x 16` patch grid and only covered patch tokens are pooled. `no clothing
detected` prevents partial skin/feet images from creating a clothing vector.

## Stored Data

```text
SQLite: persons, roles, images, face_embeddings, clothing_embeddings
Faiss:  face index (512 dimensions), clothing index (384 dimensions)
pHash:  in-memory index reconstructed from SQLite on start
```

Faiss vector IDs are SQLite embedding IDs, not image IDs. The metadata store
resolves an embedding ID back to its image, person, and optional role. Exact
matching uses image IDs directly.

## Ingest Flow

```text
source image
  -> MD5 duplicate check
  -> images row
  -> face / clothing / exact routes
  -> embeddings + Faiss entries + pHash
  -> committed image status
```

For directory ingestion, oneTBB runs a `parallel_pipeline` with seven in-flight
tokens:

```text
serial source -> parallel MD5/decode -> serial duplicate + image row
              -> parallel route inference -> serial result/progress output
```

The serial stages protect order-sensitive SQLite setup and counters. `FaissFlatIpIndex`
and `PHashIndex` protect their mutable state internally. A completed token releases
a slot immediately, so the next file starts without waiting for the whole batch.

## Progress Events

`TaskProgressHub` is a process-local publish/subscribe bridge for future UI or
service hosts:

```text
background ingest/query worker
  -> TaskProgressHub::Notify(TaskProgressEvent)
  -> registered UI/service callbacks
  -> consumer dispatches onto its own UI thread
```

Events contain a caller-selected task ID, operation, status, counters, current
file, and message. The hub invokes callbacks synchronously and catches callback
exceptions so subscribers cannot terminate an ingest/query task.

## Extension Rules

1. Add a model behind an L3 adapter rather than calling ONNX Runtime from L4/L5.
2. Add a new visual signal as an L4 route and preserve its metadata/index ID mapping.
3. Add a new cross-route policy in L5; do not make one route depend on another.
4. Any embedding preprocessing change requires a new/rebuilt compatible index.
5. Update `README.md`, `plan.md`, and regression coverage with every behavior or
   schema change.
