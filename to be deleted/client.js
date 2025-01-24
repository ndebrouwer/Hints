import './polyfill.js'; // Ensure this is loaded before anything else
import createZkEmailSdk from "@zk-email/sdk";
import promptSync from "prompt-sync";
import { config } from "dotenv";
import fs from "fs";

config(); // Loads .env variables (like ZKEMAIL_API_KEY) into process.env

// Initialize the prompt
const prompt = promptSync();
async function main() {
  // Prompt user for domain & keywords
  const claimedDomain = prompt("Enter the claimed domain (e.g. example.com): ");
  const rawKeywords = prompt("Enter keywords (comma-separated): ");
  const keywordsArray = rawKeywords.split(",").map((k) => k.trim());
  const PUBLIC_SDK_KEY = "pk_live_51NXwT8cHf0vYAjQK9LzB3pM6R8gWx2F";
  const session_token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3Mzg1MzYxNDYsImdpdGh1Yl91c2VybmFtZSI6Im5kZWJyb3V3ZXIifQ.iOVk8_SNfWaxijTj2RYm6VYcOCbmaELSu-n__K1z26w"
  const sdk = createZkEmailSdk({
    baseUrl: "https://conductor.zk.email",
    auth: {
      bearer: session_token,
    },
  });
  

  // 2) Create a "generic" blueprint that *allows* external inputs (domain, keywords).
  //    In a real app, you'd do this ONCE, store "mydomain-keywords@v1", and skip re-creating later.
  const blueprintProps = {
    slug: "mydomain-keywords",     // later available as "mydomain-keywords@v1"
    circuitName: "domain-keywords-circuit",
    title: "Domain + Body Keywords Checker",
    description: "Checks DKIM domain plus user-specified keywords in the body.",
    externalInputs: [
      { name: "requiredDomain", maxLength: 50 },
      { name: "keywords", maxLength: 200 },
    ],
    // Not using regex here, so no decomposedRegexes needed
    decomposedRegexes: [],
    isPublic: true,
    ignoreBodyHashCheck: false,
    enableHeaderMasking: false,
    enableBodyMasking: false,
  };

  // Create & submit the blueprint (draft + compile)
  const blueprint = sdk.createBlueprint(blueprintProps);
  console.log("Submitting blueprint as draft...");
  await blueprint.submitDraft();
  console.log("Compiling blueprint...");
  await blueprint.submit(); // triggers circuit compilation on backend

  // After compile, the final slug might be "mydomain-keywords@v1".
  // You could fetch it later with sdk.getBlueprint("mydomain-keywords@v1").
  const finalSlug = `${blueprintProps.slug}@1`; // Usually the version is "1" after first compile
  console.log("Blueprint compiled at slug:", finalSlug);

  // 3) Generate a proof, using the domain & keywords we prompted for.
  //    We'll just read a sample email from disk for demonstration.
  const rawEmailPath = prompt("Path to a raw email file (e.g. sample.eml): ");
  const rawEmail = fs.readFileSync(rawEmailPath, "utf8");

  // Retrieve the just-compiled blueprint (in practice, you might do this in a separate script)
  const compiledBlueprint = await sdk.getBlueprint(finalSlug);
  const prover = compiledBlueprint.createProver();

  console.log("Generating proof with domain + keywords as external inputs...");
  const proof = await prover.generateProof({
    emailFile: rawEmail,
    externalInputs: {
      requiredDomain: claimedDomain,
      keywords: keywordsArray,
    },
  });

  console.log("\nProof generated successfully:\n", proof);
  console.log("\nYou can now verify this proof on-chain or share it as needed.");
}

main().catch((err) => {
  console.error("An error occurred:", err);
});
