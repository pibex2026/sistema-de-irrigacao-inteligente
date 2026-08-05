import gzip
import shutil
from pathlib import Path

Import("env")

def prepare_and_compress_data_files(source, target, env):
    project_dir = Path(env.get("PROJECT_DIR"))
    data_dir_str = env.get("PROJECT_DATA_DIR")

    if not data_dir_str:
        print("[GZIP] ERRO: PROJECT_DATA_DIR não definido.")
        return

    view_dir = project_dir / "view"
    data_dir = Path(data_dir_str)

    if not view_dir.exists():
        print(f"[GZIP] AVISO: Pasta 'view' não encontrada em: {view_dir}")
        return

    # Garante que a pasta 'data' exista sem apagar o que já está nela
    data_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n--- [GZIP] Atualizando/Sobrescrevendo arquivos em 'data/' a partir de 'view/' ---")

    # Extensões que serão comprimidas para .gz
    extensions = {".js", ".html", ".css", ".ico", ".svg"}

    # Percorre os arquivos de 'view' e substitui em 'data'
    for file_path in view_dir.rglob("*"):
        if file_path.is_file():
            # Mapeia o caminho relativo para replicar subpastas
            relative_path = file_path.relative_to(view_dir)
            target_path = data_dir / relative_path

            # Garante que as subpastas existam em 'data/' se necessário
            target_path.parent.mkdir(parents=True, exist_ok=True)

            if file_path.suffix.lower() in extensions:
                gz_path = target_path.with_name(f"{target_path.name}.gz")

                # Abre e sobrescreve o arquivo .gz existente em 'data/'
                with open(file_path, 'rb') as f_in:
                    with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
                        shutil.copyfileobj(f_in, f_out)

                print(f"  └─ Atualizado/Sobrescrito: {relative_path} -> {gz_path.name}")
            else:
                # Copia e sobrescreve outros arquivos (ex: imagens) diretamente
                shutil.copy2(file_path, target_path)
                print(f"  └─ Copiado/Sobrescrito: {relative_path}")

    print("--- [GZIP] Concluído com sucesso! ---\n")


# Hooks do PlatformIO para LittleFS
env.AddPreAction("$BUILD_DIR/littlefs.bin", prepare_and_compress_data_files)
env.AddPreAction("buildfs", prepare_and_compress_data_files)