"""Apply the same strict, in-memory shader recipes used by the standalone DLL."""

import argparse
import hashlib
import json
from pathlib import Path


def canonical(data: bytes) -> str:
    return data.decode("utf-8-sig").replace("\r\n", "\n")


def adapt(source: str, family: str, adapter: Path) -> tuple[str, dict]:
    if "NightEye/ActorVision" in source or "NIGHT_EYE" in source:
        raise ValueError("Legacy or previously adapted Night Eye shader is unsupported")
    digest = hashlib.sha256(source.encode()).hexdigest()
    recipes = json.loads((adapter / "Recipes.json").read_text(encoding="utf-8"))
    recipe = next((item for item in recipes["recipes"]
                   if item["family"] == family and item["sourceSHA256"] == digest), None)
    if recipe is None:
        raise ValueError(f"Unrecognized {family} source: {digest}")
    result = source
    for insertion in recipe["insertions"]:
        anchor = insertion["anchor"]
        if result.count(anchor) != 1:
            raise ValueError(f"Anchor is not unique: {anchor!r}")
        body = canonical((adapter / insertion["asset"]).read_bytes())
        if insertion["asset"] == "Declarations.hlsli":
            body = f"#define NIGHT_EYE_ADAPTER_OS217 {int(recipe['os217'])}\n" + body
        position = result.index(anchor) + (len(anchor) if insertion["after"] else 0)
        result = result[:position] + "\n" + body + "\n" + result[position:]
    return result, recipe


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--adapter-root", type=Path, default=Path(__file__).resolve().parents[1] / "Shaders/NightEye/Adapter")
    args = parser.parse_args()
    if args.source_root.resolve() == args.output_root.resolve():
        raise ValueError("The validation output must differ from the host source tree")
    args.output_root.mkdir(parents=True, exist_ok=True)
    report = []
    for family in ("Lighting", "Effect"):
        source_path = args.source_root / f"{family}.hlsl"
        source_bytes = source_path.read_bytes()
        result, recipe = adapt(canonical(source_bytes), family, args.adapter_root)
        output = args.output_root / source_path.name
        output.write_text(result, encoding="utf-8", newline="\n")
        report.append({"family": family, "version": recipe["version"],
                       "source": str(source_path.resolve()), "sourceSHA256": hashlib.sha256(source_bytes).hexdigest(),
                       "canonicalSHA256": recipe["sourceSHA256"], "adaptedSHA256": hashlib.sha256(result.encode()).hexdigest(),
                       "output": str(output.resolve()), "insertions": len(recipe["insertions"])})
        if source_path.read_bytes() != source_bytes:
            raise ValueError("Host source changed during validation")
    (args.output_root / "Adaptation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
