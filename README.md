# Treasure Hunt System

This project implements a treasure hunt system with multiple processes and signal-based communication.

## Components

1. **treasure_hub** - Interactive interface for the user
2. **treasure_monitor** - Background process that monitors and interacts with treasure hunts
3. **treasure_manager** - Manages the actual treasure hunts and treasures

## Building the System

To build all components, run:

```bash
chmod +x build.sh  # Make the build script executable
./build.sh         # Build all components
```

## Using the System

Start the system by running:

```bash
./treasure_hub
```

### Available Commands

The system supports the following commands:

- **start_monitor**: Starts a separate background process that monitors the hunts
- **list_hunts**: Lists all available hunts and the number of treasures in each
- **list_treasures \<hunt_id\>**: Shows information about all treasures in a hunt
- **view_treasure \<hunt_id\> \<treasure_id\>**: Shows detailed information about a specific treasure
- **stop_monitor**: Stops the monitor process (the process will delay its exit to demonstrate proper termination handling)
- **exit**: Exits the program (only if the monitor is not running)

## Creating New Treasure Hunts

To create new hunts and add treasures for testing, you can use the treasure_manager directly:

```bash
# Create a new hunt
./treasure_manager create hunt1

# Add treasures to the hunt
./treasure_manager add hunt1 treasure1 "Gold coin from 1812"
./treasure_manager add hunt1 treasure2 "Ancient map"
./treasure_manager add hunt1 treasure3 "Diamond necklace"
```

## Implementation Details

- The system uses signals (specifically SIGUSR1 and SIGCHLD) for inter-process communication
- Command details are passed through files (monitor_command.txt and monitor_params.txt)
- The treasure_monitor intentionally delays its termination to demonstrate proper handling of commands during shutdown
