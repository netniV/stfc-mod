# Contributing: Game State Export Feature

## Overview

This document describes how to contribute to the Game State Export feature of the STFC Community Mod.

## Repository Structure

- **Upstream**: `netniV/stfc-mod` (original repository)
- **Your Fork**: `DrCord/stfc-mod` (your development fork)
- **Working Branch**: Always `dev`, NEVER `main`

## Development Workflow

### 1. Create Feature Branch

```bash
# Make sure you're on dev
git checkout dev

# Pull latest changes from upstream
git pull upstream dev

# Create feature branch
git checkout -b feature/your-feature-name
```

### 2. Make Your Changes

Edit the files, commit frequently with clear messages:

```bash
git add mods/src/patches/parts/gamestate_export.cc
git commit -m "feat(gamestate): Add building data export

- Hook into SaveSystem proto data
- Extract building IDs, names, levels
- Add error handling

Refs #XX"
```

### 3. Commit Message Format

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): short description

Longer description if needed

Refs #issue_number
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `refactor`: Code refactoring
- `test`: Adding tests
- `chore`: Maintenance

### 4. Push and Open PR

```bash
# Push to your fork
git push origin feature/your-feature-name

# Open PR at https://github.com/netniV/stfc-mod
# Base: netniV/stfc-mod:dev
# Compare: YourUsername/stfc-mod:feature/your-feature-name
```

## Before Opening PR

### Code Quality Checklist

- [ ] Code follows existing style conventions
- [ ] All includes are properly organized
- [ ] No compiler warnings
- [ ] No hardcoded paths or values
- [ ] Comprehensive error handling
- [ ] Informative log messages

### Testing Checklist

- [ ] Debug build successful
- [ ] Release build successful
- [ ] Feature disabled by default works
- [ ] Feature enabled works
- [ ] JSON output valid
- [ ] Logs are correct

## Adding Game Data Hooks

When adding actual game data (Phase 2):

1. Study existing IL2CPP bindings in `il2cpp/il2cpp-functions.cc`
2. Reference protobuf definitions in `prime/proto/`
3. Look at `sync.cc` for data access patterns
4. Add error handling for null/missing data
5. Log when data unavailable
6. Test with multiple game versions if possible

Example:

```cpp
json export_buildings()
{
  json buildings_array = json::array();

  try
  {
    // Get SaveSystem data from game
    auto save_data = GetSaveSystemData();
    if (save_data && save_data->buildings())
    {
      for (const auto& building : save_data->buildings())
      {
        buildings_array.push_back({
          {"id", building->id()},
          {"name", building->name()},
          {"level", building->level()}
        });
      }
      spdlog::debug("GameState export: Exported {} buildings", buildings_array.size());
    }
  }
  catch (const std::exception& ex)
  {
    spdlog::warn("GameState export: Failed to export buildings: {}", ex.what());
  }

  return buildings_array;
}
```

## Questions?

- **Discord**: https://discord.gg/PrpHgs7Vjs
- **GitHub Discussions**: Ask in upstream repo
- **Issues**: Create issue for questions

Happy coding! ??
