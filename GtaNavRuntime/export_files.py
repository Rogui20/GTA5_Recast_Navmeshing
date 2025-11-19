import pathlib

# Arquivos que você quer incluir no output
# Pode ser caminhos absolutos ou relativos
files = [
    "ChunkyTriMesh.cpp",
    "ChunkyTriMesh.h",
    "GtaNav.cpp",
    "GtaNavAPI.h",
    "GtaNavAPI.cpp",
    "GtaNavContext.cpp",
    "GtaNavContext.h",
    "GtaNavGeometry.cpp",
    "GtaNavGeometry.h",
    "GtaNavProps.cpp",
    "GtaNavProps.h",
    "GtaNavTiles.cpp",
    "GtaNavTiles.h",
    "GtaNavTransform.h",
    "InputGeom.cpp",
    "InputGeom.h",
    "MeshLoaderObj.h",
    "MeshLoaderObj.cpp"
]

output_path = "output.txt"

def generate_output(file_list, output):
    with open(output, "w", encoding="utf-8") as out:
        for file_path in file_list:
            p = pathlib.Path(file_path)
            if not p.exists():
                out.write(f"=== {file_path} (NOT FOUND) ===\n\n")
                continue

            out.write(f"=== {p.name} ===\n")
            out.write(p.read_text(encoding="utf-8"))
            out.write("\n\n")  # separador entre arquivos

    print(f"Arquivo '{output}' gerado com sucesso.")


if __name__ == "__main__":
    generate_output(files, output_path)
