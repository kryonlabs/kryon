// Column Alignments Demo - TypeScript/JSX
//
// Demonstrates different justify (mainAxisAlignment) values for Column layouts

import { run } from '../../bindings/typescript/src/index.ts';

// Define alignment configurations
const alignments = [
  {
    name: "start (flex-start)",
    value: "start" as const,
    gap: 15,
    colors: ["#ef4444", "#dc2626", "#b91c1c"],
  },
  {
    name: "center",
    value: "center" as const,
    gap: 15,
    colors: ["#06b6d4", "#0891b2", "#0e7490"],
  },
  {
    name: "end (flex-end)",
    value: "end" as const,
    gap: 15,
    colors: ["#8b5cf6", "#7c3aed", "#6d28d9"],
  },
  {
    name: "spaceEvenly",
    value: "space-evenly" as const,
    gap: 0,
    colors: ["#64748b", "#475569", "#334155"],
  },
  {
    name: "spaceAround",
    value: "space-around" as const,
    gap: 0,
    colors: ["#f59e0b", "#d97706", "#b45309"],
  },
  {
    name: "spaceBetween",
    value: "space-between" as const,
    gap: 0,
    colors: ["#10b981", "#059669", "#047857"],
  },
];

// Alignment column component
function AlignmentColumn({
  name,
  value,
  gap,
  colors
}: typeof alignments[number]) {
  return (
    <container width={150} height={740} backgroundColor="#ffffff" padding={15}>
      <column justify="start" align="center" gap={10}>
        <text content={name} fontSize={12} />
        <container width={120} height={600} backgroundColor="#f1f5f9" padding={8}>
          <column justify={value} align="center" gap={gap}>
            <container width={80} height={60} backgroundColor={colors[0]} />
            <container width={80} height={80} backgroundColor={colors[1]} />
            <container width={80} height={70} backgroundColor={colors[2]} />
          </column>
        </container>
      </column>
    </container>
  );
}

// Main App
function App() {
  return (
    <row
      width="100%"
      height="100%"
      backgroundColor="#f8fafc"
      justify="start"
      gap={20}
    >
      {alignments.map((alignment) => (
        <AlignmentColumn key={alignment.name} {...alignment} />
      ))}
    </row>
  );
}

run(<App />, {
  title: 'Column Alignments Demo',
  width: 1000,
  height: 800,
});
