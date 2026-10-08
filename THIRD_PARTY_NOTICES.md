# Licensing and credits

Truly Night Eye is distributed under the GNU General Public License,
version 3 or any later version. The additional permissions in
[EXCEPTIONS.md](EXCEPTIONS.md) apply to allow modding and linking with the
game and modding libraries. The full GPL text is in [LICENSE](LICENSE).

This standalone addon was developed from the Night Eye feature and graphics
integration patterns in Community Shaders and Open Shaders. Their applicable
copyright notices and modding permissions are retained. Shader-derived code
also retains the MIT notice, copyright 2022 Ilya Perapechka, in
[LICENSES/CommunityShaders-Shaders-MIT.txt](LICENSES/CommunityShaders-Shaders-MIT.txt).

Third-party libraries retain their own MIT or BSD licences. Their notices
are included in [LICENSES](LICENSES). Build dependencies are pinned in
`CMakeLists.txt` and `vcpkg.json`; see [BUILDING.md](BUILDING.md).

The eyeshine textures are generated monochrome region masks. Their editable
sources are in `assets/eyeshine`. Game meshes, creature textures, captures,
personal settings and shader caches are not included.
